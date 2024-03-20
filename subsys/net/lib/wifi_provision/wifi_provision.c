/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <stdio.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/dhcpv4_server.h>
#include <net/wifi_mgmt_ext.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <net/wifi_credentials.h>
#include <zephyr/smf.h>
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/posix/unistd.h>
#include <zephyr/posix/sys/socket.h>
#include <zephyr/net/tls_credentials.h>
#include <zephyr/net/dns_sd.h>
#include <net/wifi_provision.h>

#include <zephyr/net/http/client.h>
#include <zephyr/net/http/parser.h>

#include "pb_decode.h"
#include "pb_encode.h"
#include "proto/common.pb.h"

LOG_MODULE_REGISTER(wifi_provision, CONFIG_WIFI_PROVISION_LOG_LEVEL);

/* HTTP responses for demonstration */
#define RESPONSE_200 "HTTP/1.1 200 OK\r\n"
#define RESPONSE_400 "HTTP/1.1 400 Bad Request\r\n\r\n"
#define RESPONSE_403 "HTTP/1.1 403 Forbidden\r\n\r\n"
#define RESPONSE_404 "HTTP/1.1 404 Not Found\r\n\r\n"
#define RESPONSE_405 "HTTP/1.1 405 Method Not Allowed\r\n\r\n"
#define RESPONSE_500 "HTTP/1.1 500 Internal Server Error\r\n\r\n"

/* Zephyr NET management events that this module subscribes to. */
#define NET_MGMT_WIFI (NET_EVENT_WIFI_AP_ENABLE_RESULT		| \
		       NET_EVENT_WIFI_AP_DISABLE_RESULT		| \
		       NET_EVENT_WIFI_AP_STA_CONNECTED		| \
		       NET_EVENT_WIFI_SCAN_DONE			| \
		       NET_EVENT_WIFI_SCAN_RESULT		| \
		       NET_EVENT_WIFI_CONNECT_RESULT		| \
		       NET_EVENT_WIFI_AP_STA_DISCONNECTED)

static struct net_mgmt_event_callback net_l2_mgmt_cb;

/* Forward declarations */
static int ap_enable(void);

/* Externs */
extern char *net_sprint_ll_addr_buf(const uint8_t *ll, uint8_t ll_len, char *buf, int buflen);

/* Register service */
DNS_SD_REGISTER_TCP_SERVICE(wifi_provision_sd, CONFIG_NET_HOSTNAME, "_http", "local",
			    DNS_SD_EMPTY_TXT, CONFIG_WIFI_PROVISION_TCP_PORT);

/* Internal variables */
static ScanResults scan = ScanResults_init_zero;
static uint8_t scan_result_buffer[1024];
static size_t scan_result_buffer_len;
static const struct smf_state state[];
static struct http_parser_settings parser_settings;

/* Semaphore used to block wifi_provision_start() until provisioning has completed. */
static K_SEM_DEFINE(wifi_provision_sem, 0, 1);

/* Variable used to indicated that we have received and stored credentials, used to break
 * out of socket recv.
 */
static bool credentials_stored;

/* Local reference to callers handler, used to send events to the application. */
static wifi_provision_evt_handler_t handler_cb;

/* Module events used to trigger state transitions. */
enum module_event {
	EVENT_AP_ENABLE = 0x1,
	EVENT_AP_DISABLE,
	EVENT_SCAN_DONE,
	EVENT_CREDENTIALS_RECEIVED,
	EVENT_RESET
};

/* Zephyr State Machine Framework variables */
enum module_state {
	STATE_UNPROVISIONED,
	STATE_PROVISIONING,
	STATE_PROVISIONED,
	STATE_FINISHED,
	STATE_RESET
};

/* Message queue used to schedule state transitions. */
K_MSGQ_DEFINE(wifi_provision_msgq, sizeof(enum module_event),
	      CONFIG_WIFI_PROVISION_MESSAGE_QUEUE_ENTRIES, 1);

/* User defined state object used with SMF.
 * Used to transfer data between state changes.
 */
static struct s_object {
	/* This must be first */
	struct smf_ctx ctx;

	/* State machine variables */
	enum module_event event_next;

	/* Provisioning parameters */
	char ssid[50];
	char psk[50];
} state_object;

/* Structure used to parse incoming HTTP requests. */
static struct http_req {
	struct http_parser parser;
	int socket;
	int accepted;
	bool received_all;
	enum http_method method;
	const char *url;
	size_t url_len;
	const char *body;
	size_t body_len;
} request;

/* Self-signed server certificate. */
static const unsigned char server_certificate[] = {
	#include "server_certificate.pem.inc"
	(0x00)
};

static const unsigned char server_private_key[] = {
	#include "server_private_key.pem.inc"
	(0x00)
};

/* Function to notify application */
static void notify_app(enum wifi_provision_evt_type type)
{
	if (handler_cb) {
		struct wifi_provision_evt evt = {
			.type = type,
		};

		handler_cb(&evt);
	}
}

/* Set new event and add event to message queue. */
static void new_event(enum module_event event)
{
	int ret;
	enum module_event event_new = event;

	ret = k_msgq_put(&wifi_provision_msgq, &event_new, K_NO_WAIT);
	if (ret) {
		LOG_ERR("k_msgq_put, error: %d", ret);
		notify_app(WIFI_PROVISION_EVT_FATAL_ERROR);
	}
}

static void handle_wifi_scan_result(struct net_mgmt_event_callback *cb)
{
	const struct wifi_scan_result *entry = (const struct wifi_scan_result *)cb->info;

	if (scan.results_count < ARRAY_SIZE(scan.results)) {

		/* SSID */
		strncpy(scan.results[scan.results_count].ssid, entry->ssid, sizeof(entry->ssid));
		scan.results[scan.results_count].ssid[sizeof(entry->ssid) - 1] = '\0';

		/* BSSID (MAC) */
		net_sprint_ll_addr_buf(entry->mac, WIFI_MAC_ADDR_LEN,
				       scan.results[scan.results_count].bssid,
				       sizeof(scan.results[scan.results_count].bssid));
		scan.results[scan.results_count].bssid[sizeof(entry->mac) - 1] = '\0';

		/* Band */
		scan.results[scan.results_count].band =
			(entry->band == WIFI_FREQ_BAND_2_4_GHZ) ? Band_BAND_2_4_GHZ :
			(entry->band == WIFI_FREQ_BAND_5_GHZ) ? Band_BAND_5_GHZ :
			(entry->band == WIFI_FREQ_BAND_6_GHZ) ? Band_BAND_6_GHZ :
			Band_BAND_UNSPECIFIED;

		/* Channel */
		scan.results[scan.results_count].channel = entry->channel;

		/* Auth mode - defaults to AuthMode_WPA_WPA2_PSK. */
			scan.results[scan.results_count].authMode =
			(entry->security == WIFI_SECURITY_TYPE_NONE) ? AuthMode_OPEN :
			(entry->security == WIFI_SECURITY_TYPE_PSK) ? AuthMode_WPA_WPA2_PSK :
			(entry->security == WIFI_SECURITY_TYPE_PSK_SHA256) ? AuthMode_WPA2_PSK :
			(entry->security == WIFI_SECURITY_TYPE_SAE) ? AuthMode_WPA3_PSK :
			AuthMode_WPA_WPA2_PSK;

		/* Signal strength */
		scan.results[scan.results_count].rssi = entry->rssi;

		scan.results_count++;
	}
}

static void dhcp_server_start(void)
{
	int ret;
	uint32_t address_int;
	struct in_addr address;
	struct in_addr netmask;
	struct net_if *iface = net_if_get_first_wifi();

	/* Set base address for DHCPv4 server based on statically assigned IPv4 address. */
	if (inet_pton(AF_INET, CONFIG_WIFI_PROVISION_IPV4_ADDRESS, &address) != 1) {
		LOG_ERR("Failed to convert IPv4 address");
		notify_app(WIFI_PROVISION_EVT_FATAL_ERROR);
		return;
	}

	/* Manually set IPv4 address */
	if (net_if_ipv4_addr_add(iface, &address, NET_ADDR_OVERRIDABLE, 0) == NULL) {
		LOG_ERR("Failed to add IPv4 address to interface");
		notify_app(WIFI_PROVISION_EVT_FATAL_ERROR);
		return;
	}

	LOG_DBG("IPv4 address added to interface");

	/* Manually set IPv4 netmask. */
	if (inet_pton(AF_INET, CONFIG_WIFI_PROVISION_IPV4_NETMASK, &netmask) != 1) {
		LOG_ERR("Failed to convert netmask");
		notify_app(WIFI_PROVISION_EVT_FATAL_ERROR);
		return;
	}

	net_if_ipv4_set_netmask(iface, &netmask);

	LOG_DBG("IPv4 netmask set");

	/* Increment base address by 1 to avoid collision with the base address. */
	address_int = ntohl(address.s_addr);
	address_int += 1;
	address.s_addr = htonl(address_int);

	ret = net_dhcpv4_server_start(iface, &address);
	if (ret) {
		LOG_ERR("Failed to start DHCPv4 server, error: %d", ret);
		notify_app(WIFI_PROVISION_EVT_FATAL_ERROR);
		return;
	}

	LOG_DBG("DHCPv4 server started");
}

static void net_mgmt_wifi_event_handler(struct net_mgmt_event_callback *cb, uint32_t mgmt_event,
					struct net_if *iface)
{
	switch (mgmt_event) {
	case NET_EVENT_WIFI_AP_ENABLE_RESULT:
		LOG_DBG("NET_EVENT_WIFI_AP_ENABLE_RESULT");

		new_event(EVENT_AP_ENABLE);
		break;
	case NET_EVENT_WIFI_AP_DISABLE_RESULT:
		LOG_DBG("NET_EVENT_WIFI_AP_DISABLE_RESULT");

		new_event(EVENT_AP_DISABLE);
		break;
	case NET_EVENT_WIFI_AP_STA_CONNECTED:
		LOG_DBG("NET_EVENT_WIFI_AP_STA_CONNECTED");

		notify_app(WIFI_PROVISION_EVT_CLIENT_CONNECTED);
		break;
	case NET_EVENT_WIFI_AP_STA_DISCONNECTED:
		LOG_DBG("NET_EVENT_WIFI_AP_STA_DISCONNECTED");

		notify_app(WIFI_PROVISION_EVT_CLIENT_DISCONNECTED);
		break;
	case NET_EVENT_WIFI_SCAN_RESULT:
		handle_wifi_scan_result(cb);
		break;
	case NET_EVENT_WIFI_SCAN_DONE:
		LOG_DBG("NET_EVENT_WIFI_SCAN_DONE");

		/* Scan results cached, waiting for client to request available networks and
		 * provide Wi-Fi credentials.
		 */
		new_event(EVENT_SCAN_DONE);
		break;
	default:
		break;
	}
}

static int wifi_scan(void)
{
	int ret;
	struct net_if *iface = net_if_get_first_wifi();
	struct wifi_scan_params params = { 0 };

	LOG_DBG("Scanning for Wi-Fi networks...");

	/* Clear scan result buffer */
	memset(scan_result_buffer, 0, sizeof(scan_result_buffer));

	ret = net_mgmt(NET_REQUEST_WIFI_SCAN, iface, &params, sizeof(params));
	if (ret) {
		LOG_ERR("Failed to start Wi-Fi scan, error: %d", ret);
		return -ENOEXEC;
	}

	return 0;
}

static int ap_enable(void)
{
	int ret;
	struct net_if *iface = net_if_get_first_wifi();
	static struct wifi_connect_req_params params = { 0 };

	params.timeout = SYS_FOREVER_MS;
	params.band = WIFI_FREQ_BAND_UNKNOWN;
	params.channel = WIFI_CHANNEL_ANY;
	params.security = WIFI_SECURITY_TYPE_NONE;
	params.mfp = WIFI_MFP_OPTIONAL;
	params.ssid = CONFIG_WIFI_PROVISION_SSID;
	params.ssid_length = strlen(CONFIG_WIFI_PROVISION_SSID);

	ret = net_mgmt(NET_REQUEST_WIFI_AP_ENABLE, iface, &params,
		       sizeof(struct wifi_connect_req_params));
	if (ret) {
		LOG_ERR("Failed to enable AP, error: %d", ret);
		return -ENOEXEC;
	}

	return 0;
}

static int ap_disable(void)
{
	int ret;
	struct net_if *iface = net_if_get_first_wifi();

	ret = net_mgmt(NET_REQUEST_WIFI_AP_DISABLE, iface, NULL, 0);
	if (ret) {
		LOG_ERR("Failed to disable AP, error: %d", ret);
		return -ENOEXEC;
	}

	return 0;
}

/* Update this function to parse using NanoPB. */
static int parse_and_store_credentials(const char *body, size_t body_len)
{
	int ret;
	WifiConfig credentials = WifiConfig_init_zero;

	pb_istream_t stream = pb_istream_from_buffer((const pb_byte_t *)body, body_len);

	if (!pb_decode(&stream, WifiConfig_fields, &credentials)) {
		LOG_ERR("Decoding credentials failed");
		return -1;
	}

	LOG_DBG("Received Wi-Fi credentials: %s, %s, sectype: %d, channel: %d, band: %d",
		credentials.ssid,
		credentials.passphrase,
		credentials.authMode,
		credentials.channel,
		credentials.band);

	enum wifi_security_type sec_type =
		credentials.authMode == AuthMode_WPA_WPA2_PSK ? WIFI_SECURITY_TYPE_PSK :
		credentials.authMode == AuthMode_WPA2_PSK ? WIFI_SECURITY_TYPE_PSK_SHA256 :
		credentials.authMode == AuthMode_WPA3_PSK ? WIFI_SECURITY_TYPE_SAE :
		WIFI_SECURITY_TYPE_NONE;

	/* Preferred band is set using a flag. */
	uint32_t flag = (credentials.band == Band_BAND_2_4_GHZ) ? WIFI_CREDENTIALS_FLAG_2_4GHz :
			(credentials.band == Band_BAND_5_GHZ) ? WIFI_CREDENTIALS_FLAG_5GHz : 0;

	ret = wifi_credentials_set_personal(credentials.ssid, strlen(credentials.ssid),
					    sec_type, NULL, 0,
					    credentials.passphrase, strlen(credentials.passphrase),
					    flag, credentials.channel);
	if (ret) {
		LOG_ERR("Storing credentials failed, error: %d", ret);
		return ret;
	}

	notify_app(WIFI_PROVISION_EVT_CREDENTIALS_RECEIVED);

	return 0;
}

/* Zephyr State Machine Framework functions. */

/* Scan and cache Wi-Fi results in unprovisioned state. */
static void unprovisioned_entry(void *o)
{
	ARG_UNUSED(o);

	int ret = wifi_scan();

	if (ret) {
		LOG_ERR("wifi_scan, error: %d", ret);
		notify_app(WIFI_PROVISION_EVT_FATAL_ERROR);
		return;
	}
}

static void unprovisioned_run(void *o)
{
	struct s_object *user_object = o;

	if (user_object->event_next == EVENT_SCAN_DONE) {
		smf_set_state(SMF_CTX(&state_object), &state[STATE_PROVISIONING]);
	} else if (user_object->event_next == EVENT_RESET) {
		smf_set_state(SMF_CTX(&state_object), &state[STATE_RESET]);
	} else {
		LOG_DBG("Unknown event, skipping state transition.");
	}
}

static void unprovisioned_exit(void *o)
{
	ARG_UNUSED(o);

	LOG_DBG("Scanning for Wi-Fi networks completed, preparing protobuf payload");

	/* If we have received all scan results, copy them to a buffer and
	 * notify the application.
	 */
	pb_ostream_t stream = pb_ostream_from_buffer(scan_result_buffer,
						     sizeof(scan_result_buffer));
	if (!pb_encode(&stream, ScanResults_fields, &scan)) {
		LOG_ERR("Encoding scan results failed");
		notify_app(WIFI_PROVISION_EVT_FATAL_ERROR);
		return;
	}

	LOG_DBG("Protobuf payload prepared, scan results encoded, size: %d", stream.bytes_written);

	scan_result_buffer_len = stream.bytes_written;
}

/* Scan for available Wi-Fi networks when we enter the provisioning state. */
static void provisioning_entry(void *o)
{
	ARG_UNUSED(o);

	LOG_DBG("Enabling AP mode to allow client to connect and provide Wi-Fi credentials.");
	LOG_DBG("Waiting for Wi-Fi credentials...");

	int ret = ap_enable();

	if (ret) {
		LOG_ERR("ap_enable, error: %d", ret);
		notify_app(WIFI_PROVISION_EVT_FATAL_ERROR);
		return;
	}

	/* Notify application that provisioning has started. */
	notify_app(WIFI_PROVISION_EVT_STARTED);
}

static void provisioning_run(void *o)
{
	struct s_object *user_object = o;

	if (user_object->event_next == EVENT_AP_ENABLE) {
		/* Start DHCP server */
		dhcp_server_start();
	} else if (user_object->event_next == EVENT_CREDENTIALS_RECEIVED) {
		smf_set_state(SMF_CTX(&state_object), &state[STATE_PROVISIONED]);
	} else if (user_object->event_next == EVENT_RESET) {
		smf_set_state(SMF_CTX(&state_object), &state[STATE_RESET]);
	} else {
		LOG_DBG("Unknown event, skipping state transition.");
	}
}

/*Wi-Fi credentials received, provisioned has completed, cleanup and disable AP mode. */
static void provisioned_entry(void *o)
{
	ARG_UNUSED(o);

	LOG_DBG("Credentials received, cleaning up...");

	ARG_UNUSED(o);

	int ret;
	struct net_if *iface = net_if_get_first_wifi();

	ret = net_dhcpv4_server_stop(iface);
	if (ret) {
		LOG_ERR("Failed to stop DHCPv4 server, error: %d", ret);
		notify_app(WIFI_PROVISION_EVT_FATAL_ERROR);
		return;
	}

	ret = ap_disable();
	if (ret) {
		LOG_ERR("ap_disable, error: %d", ret);
		notify_app(WIFI_PROVISION_EVT_FATAL_ERROR);
		return;
	}
}

static void provisioned_run(void *o)
{
	struct s_object *user_object = o;

	if (user_object->event_next == EVENT_AP_DISABLE) {
		smf_set_state(SMF_CTX(&state_object), &state[STATE_FINISHED]);
	} else if (user_object->event_next == EVENT_RESET) {
		smf_set_state(SMF_CTX(&state_object), &state[STATE_RESET]);
	} else {
		LOG_DBG("Unknown event, skipping state transition.");
	}
}

/* We have received Wi-Fi credentials and is considered provisioned, disable AP mode. */
static void finished_entry(void *o)
{
	ARG_UNUSED(o);

	notify_app(WIFI_PROVISION_EVT_COMPLETED);

	/* Block init until softAP mode is disabled. */
	k_sem_give(&wifi_provision_sem);
}

static void finished_run(void *o)
{
	struct s_object *user_object = o;

	if (user_object->event_next == EVENT_RESET) {
		smf_set_state(SMF_CTX(&state_object), &state[STATE_RESET]);
	} else {
		LOG_DBG("Unknown event, skipping state transition.");
	}
}

/* Delete Wi-Fi credentials upon exit of the provisioning state. */
static void reset_entry(void *o)
{
	int ret;

	LOG_DBG("Exiting unprovisioned state, cleaning up and deleting stored Wi-Fi credentials");
	LOG_DBG("Deleting stored credentials...");

	ret = wifi_credentials_delete_all();
	if (ret) {
		LOG_ERR("wifi_credentials_delete_all, error: %d", ret);
		notify_app(WIFI_PROVISION_EVT_FATAL_ERROR);
		return;
	}

	/* Request reboot of the firmware to reset the device firmware and re-enter
	 * provisioning. Ideally we would bring the iface down/up but this is currently not
	 * supported.
	 */
	LOG_DBG("Wi-Fi credentials deleted, request reboot to re-enter provisioning (softAP mode)");

	notify_app(WIFI_PROVISION_EVT_RESET_REBOOT_REQUEST);
}

/* Construct state table */
static const struct smf_state state[] = {
	[STATE_UNPROVISIONED] = SMF_CREATE_STATE(unprovisioned_entry,
						 unprovisioned_run,
						 unprovisioned_exit),
	[STATE_PROVISIONING]  = SMF_CREATE_STATE(provisioning_entry,
						 provisioning_run,
						 NULL),
	[STATE_PROVISIONED]   = SMF_CREATE_STATE(provisioned_entry,
						 provisioned_run,
						 NULL),
	[STATE_FINISHED]      = SMF_CREATE_STATE(finished_entry,
						 finished_run,
						 NULL),
	[STATE_RESET]	      = SMF_CREATE_STATE(reset_entry,
						 NULL,
						 NULL),
};

static int send_response(struct http_req *request, char *response, size_t len)
{
	ssize_t out_len;

	while (len) {
		out_len = send(request->accepted, response, len, 0);
		if (out_len < 0) {
			LOG_ERR("send, error: %d", -errno);
			return -errno;
		}

		len -= out_len;
	}

	return 0;
}

/* Handle HTTP request */
static int handle_http_request(struct http_req *request)
{
	int ret;
	char response[2048] = { 0 };

	if ((strncmp(request->url, "/prov/networks", request->url_len) == 0)) {
		/* Wi-Fi scan requested, return the cached scan results. */

		ret = snprintk(response, sizeof(response),
			"%sContent-Type: application/x-protobuf\r\nContent-Length: %d\r\n\r\n",
			RESPONSE_200, scan_result_buffer_len);
		if ((ret < 0) || (ret >= sizeof(response))) {
			LOG_DBG("snprintk, error: %d", ret);
			return ret;
		}

		/* Send headers */
		ret = send_response(request, response, strlen(response));
		if (ret) {
			LOG_ERR("send_response (headers), error: %d", ret);
			return ret;
		}

		/* Send payload */
		ret = send_response(request, scan_result_buffer, scan_result_buffer_len);
		if (ret) {
			LOG_ERR("send_response (payload), error: %d", ret);
			return ret;
		}

	} else if ((strncmp(request->url, "/prov/configure", request->url_len) == 0)) {
		/* Wi-Fi provisioning requested, parse the body and store the credentials. */

		ret = parse_and_store_credentials(request->body, request->body_len);
		if (ret) {
			LOG_ERR("parse_and_store_credentials, error: %d", ret);
			return ret;
		}

		ret = snprintk(response, sizeof(response), "%sContent-Length: %d\r\n\r\n",
			       RESPONSE_200, 0);
		if ((ret < 0) || (ret >= sizeof(response))) {
			LOG_DBG("snprintk, error: %d", ret);
			return ret;
		}

		ret = send_response(request, response, strlen(response));
		if (ret) {
			LOG_ERR("send_response, error: %d", ret);
			return ret;
		}

		/* Wait for a second to allow the client to receive the TCP ACK. */
		k_sleep(K_SECONDS(1));

		credentials_stored = true;

		new_event(EVENT_CREDENTIALS_RECEIVED);
	} else {
		LOG_DBG("Unrecognized HTTP resource, ignoring...");
	}

	return 0;
}

static int process_tcp(sa_family_t family)
{
	int client;
	struct sockaddr_in6 client_addr;
	socklen_t client_addr_len = sizeof(client_addr);
	char addr_str[INET6_ADDRSTRLEN];
	int received;
	int ret = 0;
	char buf[2048];
	size_t offset = 0;
	size_t total_received = 0;

	client = accept(request.socket, (struct sockaddr *)&client_addr, &client_addr_len);
	if (client < 0) {
		LOG_ERR("Error in accept %d, try again", -errno);
		return -errno;
	}

	request.accepted = client;

	net_addr_ntop(client_addr.sin6_family, &client_addr.sin6_addr, addr_str, sizeof(addr_str));
	LOG_DBG("[%d] Connection from %s accepted", request.accepted, addr_str);

	http_parser_init(&request.parser, HTTP_REQUEST);

	while (true) {
		received = recv(request.accepted, buf + offset, sizeof(buf) - offset, 0);
		if (received == 0) {
			/* Connection closed */
			ret = -ECONNRESET;
			LOG_DBG("[%d] Connection closed by peer", request.accepted);
			goto socket_close;
		} else if (received < 0) {
			/* Socket error */
			ret = -errno;
			LOG_ERR("[%d] Connection error %d", request.accepted, ret);
			goto socket_close;
		}

		/* Parse the received data as HTTP request */
		(void)http_parser_execute(&request.parser,
					  &parser_settings, buf + offset, received);

		total_received += received;
		offset += received;

		if (offset >= sizeof(buf)) {
			offset = 0;
		}

		/* If the HTTP request has been completely received, stop receiving data and
		 * proceed to process the request.
		 */
		if (request.received_all) {
			handle_http_request(&request);
			break;
		}
	};

socket_close:
	LOG_DBG("Closing listening socket: %d", request.accepted);
	(void)close(request.accepted);

	return ret;
}

static int setup_server(int *sock, struct sockaddr *bind_addr, socklen_t bind_addrlen)
{
	int ret;
	int enable = 1;

	*sock = socket(bind_addr->sa_family, SOCK_STREAM, IPPROTO_TLS_1_2);
	if (*sock < 0) {
		LOG_ERR("Failed to create socket: %d", -errno);
		return -errno;
	}

	sec_tag_t sec_tag_list[] = {
		CONFIG_WIFI_PROVISION_CERTIFICATE_SEC_TAG,
	};

	ret = setsockopt(*sock, SOL_TLS, TLS_SEC_TAG_LIST, sec_tag_list, sizeof(sec_tag_list));
	if (ret < 0) {
		LOG_ERR("Failed to set security tag list %d", -errno);
		return -errno;
	}

	ret = setsockopt(*sock, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
	if (ret) {
		LOG_ERR("Failed to set SO_REUSEADDR %d", -errno);
		return -errno;
	}

	ret = bind(*sock, bind_addr, bind_addrlen);
	if (ret < 0) {
		LOG_ERR("Failed to bind socket %d", -errno);
		return -errno;
	}

	ret = listen(*sock, 1);
	if (ret < 0) {
		LOG_ERR("Failed to listen on socket %d", -errno);
		return -errno;
	}

	return ret;
}

/* Processing incoming IPv4 clients */
static int process_tcp4(void)
{
	int ret;
	struct sockaddr_in addr4 = {
		.sin_family = AF_INET,
		.sin_port = htons(CONFIG_WIFI_PROVISION_TCP_PORT),
	};

	ret = setup_server(&request.socket, (struct sockaddr *)&addr4, sizeof(addr4));
	if (ret < 0) {
		LOG_ERR("Failed to create IPv4 socket %d", ret);
		return ret;
	}

	LOG_DBG("Waiting for IPv4 HTTP connections on port %d", CONFIG_WIFI_PROVISION_TCP_PORT);

	while (true) {

		/* Process incoming IPv4 clients */
		ret = process_tcp(AF_INET);
		if (ret < 0) {
			LOG_ERR("Failed to process TCP %d", ret);
			return ret;
		}

		if (credentials_stored) {
			LOG_DBG("Credentials stored, closing server socket");
			close(request.socket);
			break;
		}
	}

	return 0;
}

/* HTTP parser callbacks */
static int on_body(struct http_parser *parser, const char *at, size_t length)
{
	struct http_req *req = CONTAINER_OF(parser, struct http_req, parser);

	req->body = at;
	req->body_len = length;

	LOG_DBG("on_body: %d", parser->method);
	LOG_DBG("> %.*s", length, at);

	return 0;
}

static int on_headers_complete(struct http_parser *parser)
{
	struct http_req *req = CONTAINER_OF(parser, struct http_req, parser);

	req->method = parser->method;

	LOG_DBG("on_headers_complete, method: %s", http_method_str(parser->method));

	return 0;
}

static int on_message_begin(struct http_parser *parser)
{
	struct http_req *req = CONTAINER_OF(parser, struct http_req, parser);

	req->received_all = false;

	LOG_DBG("on_message_begin, method: %d", parser->method);

	return 0;
}

static int on_message_complete(struct http_parser *parser)
{
	struct http_req *req = CONTAINER_OF(parser, struct http_req, parser);

	req->received_all = true;

	LOG_DBG("on_message_complete, method: %d", parser->method);

	return 0;
}

static int on_url(struct http_parser *parser, const char *at, size_t length)
{
	struct http_req *req = CONTAINER_OF(parser, struct http_req, parser);

	req->url = at;
	req->url_len = length;

	LOG_DBG("on_url, method: %d", parser->method);
	LOG_DBG("> %.*s", length, at);

	return 0;
}

int wifi_provision_start(const wifi_provision_evt_handler_t handler)
{
	int ret;

	/* Set library callback handler */
	handler_cb = handler;

	if (!wifi_credentials_is_empty()) {
		LOG_DBG("Stored Wi-Fi credentials found, already provisioned");
		smf_set_initial(SMF_CTX(&state_object), &state[STATE_FINISHED]);
		return 0;
	}

	/* Provision self-signed server certificate */
	ret = tls_credential_add(CONFIG_WIFI_PROVISION_CERTIFICATE_SEC_TAG,
				 TLS_CREDENTIAL_CA_CERTIFICATE,
				 server_certificate,
				 sizeof(server_certificate));
	if (ret == -EEXIST) {
		LOG_DBG("CA already exists, sec tag: %d",
			CONFIG_WIFI_PROVISION_CERTIFICATE_SEC_TAG);
	} else if (ret < 0) {
		LOG_ERR("Failed to register CA certificate: %d", ret);
		return ret;
	}

	ret = tls_credential_add(CONFIG_WIFI_PROVISION_CERTIFICATE_SEC_TAG,
				 TLS_CREDENTIAL_SERVER_CERTIFICATE,
				 server_certificate,
				 sizeof(server_certificate));
	if (ret == -EEXIST) {
		LOG_DBG("Public certificate already exists, sec tag: %d",
			CONFIG_WIFI_PROVISION_CERTIFICATE_SEC_TAG);
	} else if (ret < 0) {
		LOG_ERR("Failed to register public certificate: %d", ret);
		return ret;
	}

	ret = tls_credential_add(CONFIG_WIFI_PROVISION_CERTIFICATE_SEC_TAG,
				 TLS_CREDENTIAL_PRIVATE_KEY,
				 server_private_key, sizeof(server_private_key));

	if (ret == -EEXIST) {
		LOG_DBG("Private key already exists, sec tag: %d",
			CONFIG_WIFI_PROVISION_CERTIFICATE_SEC_TAG);
	} else if (ret < 0) {
		LOG_ERR("Failed to register private key: %d", ret);
		return ret;
	}

	LOG_DBG("Self-signed server certificate provisioned");

	net_mgmt_init_event_callback(&net_l2_mgmt_cb,
				     net_mgmt_wifi_event_handler,
				     NET_MGMT_WIFI);

	net_mgmt_add_event_callback(&net_l2_mgmt_cb);

	smf_set_initial(SMF_CTX(&state_object), &state[STATE_UNPROVISIONED]);

	http_parser_settings_init(&parser_settings);

	parser_settings.on_body = on_body;
	parser_settings.on_headers_complete = on_headers_complete;
	parser_settings.on_message_begin = on_message_begin;
	parser_settings.on_message_complete = on_message_complete;
	parser_settings.on_url = on_url;

	/* Start processing incoming IPv4 clients */
	ret = process_tcp4();
	if (ret < 0) {
		LOG_ERR("Failed to start TCP server %d", ret);
		return ret;
	}

	k_sem_take(&wifi_provision_sem, K_FOREVER);
	return 0;
}

int wifi_provision_reset(void)
{
	LOG_DBG("Resetting Wi-Fi provision state machine");
	new_event(EVENT_RESET);
	return 0;
}

/* Function used to schedule state transitions. State transitions are offloaded to a separate
 * workqueue to avoid races with Zephyr NET management. Time consuming tasks can safely be offloaded
 * to this workqueue.
 */
static void wifi_provision_task(void)
{
	int ret;
	enum module_event event_new = 0;

	while (1) {

		ret = k_msgq_get(&wifi_provision_msgq, &event_new, K_FOREVER);
		if (ret) {
			LOG_ERR("k_msgq_get, error: %d", ret);
			notify_app(WIFI_PROVISION_EVT_FATAL_ERROR);
			return;
		}

		state_object.event_next = event_new;

		ret = smf_run_state(SMF_CTX(&state_object));
		if (ret) {
			LOG_ERR("smf_run_state, error: %d", ret);
			notify_app(WIFI_PROVISION_EVT_FATAL_ERROR);
			return;
		}
	}
}

K_THREAD_DEFINE(wifi_provision_task_id, 8192,
		wifi_provision_task, NULL, NULL, NULL, 3, 0, 0);
