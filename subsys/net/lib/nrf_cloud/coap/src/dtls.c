/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/net/coap.h>
#if defined(CONFIG_POSIX_API)
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/posix/sys/socket.h>
#else
#include <zephyr/net/socket.h>
#endif
#include <modem/lte_lc.h>
#include <modem/modem_key_mgmt.h>
#if defined(CONFIG_MODEM_INFO)
#include <modem/modem_info.h>
#endif
#include <nrf_socket.h>
#include <nrf_modem_at.h>

#include "dtls.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(dtls, CONFIG_NRF_CLOUD_COAP_LOG_LEVEL);

static int sectag = CONFIG_NRF_CLOUD_COAP_SEC_TAG;
static bool dtls_cid_active;
static bool mfw_has_cid = true;

#if defined(CONFIG_MODEM_INFO)
static struct modem_param_info mdm_param;

static int get_modem_info(void)
{
	int err;

	err = modem_info_string_get(MODEM_INFO_IMEI,
				    mdm_param.device.imei.value_string,
				    MODEM_INFO_MAX_RESPONSE_SIZE);
	if (err <= 0) {
		LOG_ERR("Could not get IMEI: %d", err);
		return err;
	}

	err = modem_info_string_get(MODEM_INFO_FW_VERSION,
				    mdm_param.device.modem_fw.value_string,
				    MODEM_INFO_MAX_RESPONSE_SIZE);
	if (err <= 0) {
		LOG_ERR("Could not get mfw ver: %d", err);
		return err;
	}

	LOG_INF("IMEI:                    %s", mdm_param.device.imei.value_string);
	LOG_INF("Modem FW version:        %s", mdm_param.device.modem_fw.value_string);

	return 0;
}

#endif /* CONFIG_MODEM_INFO */

static int get_device_ip_address(uint8_t *d4_addr)
{
#if defined(CONFIG_MODEM_INFO)
	int err;

	err = modem_info_init();
	if (err) {
		return err;
	}

	err = modem_info_string_get(MODEM_INFO_IP_ADDRESS,
				    mdm_param.network.ip_address.value_string,
				    MODEM_INFO_MAX_RESPONSE_SIZE);
	if (err <= 0) {
		LOG_ERR("Could not get IP addr: %d", err);
		return err;
	}
	err = inet_pton(AF_INET, mdm_param.network.ip_address.value_string, d4_addr);
	if (err == 1) {
		return 0;
	}
	return errno;
#else
	d4_addr[0] = 0;
	d4_addr[1] = 0;
	d4_addr[2] = 0;
	d4_addr[3] = 0;
	return 0;
#endif

}

int dtls_init(int sock)
{
	int err;
#if defined(RESTRICT_CIPHERS)
	static const int ciphers[] = {
		MBEDTLS_TLS_PSK_WITH_AES_128_CCM_8, 0
	};
#endif

	uint8_t d4_addr[4];

	/* once connected, cache the connection info */
	dtls_cid_active = false;

#if defined(CONFIG_MODEM_INFO)
	err = get_modem_info();
	if (err) {
		LOG_INF("Modem firmware version not known");
	}
#endif

	err = get_device_ip_address(d4_addr);
	if (!err) {
		LOG_INF("Client IP address: %u.%u.%u.%u",
			d4_addr[0], d4_addr[1], d4_addr[2], d4_addr[3]);
	}

	LOG_INF("Setting socket options:");

	LOG_INF("  hostname: %s", CONFIG_NRF_CLOUD_COAP_SERVER_HOSTNAME);
	err = setsockopt(sock, SOL_TLS, TLS_HOSTNAME, CONFIG_NRF_CLOUD_COAP_SERVER_HOSTNAME,
			 sizeof(CONFIG_NRF_CLOUD_COAP_SERVER_HOSTNAME));
	if (err) {
		LOG_ERR("Error setting hostname: %d", errno);
		return err;
	}

	LOG_INF("  sectag: %d", sectag);
	err = setsockopt(sock, SOL_TLS, TLS_SEC_TAG_LIST, &sectag, sizeof(sectag));
	if (err) {
		LOG_ERR("Error setting sectag list: %d", errno);
		return err;
	}

#if defined(RESTRICT_CIPHERS)
	LOG_INF("  restrict ciphers");
	err = setsockopt(sock, SOL_TLS, TLS_CIPHERSUITE_LIST, ciphers, sizeof(ciphers));
	if (err) {
		LOG_ERR("Error setting cipherlist: %d", errno);
		return err;
	}
#endif
#if defined(DUMP_CIPHERLIST)
	int len;
	int ciphers[32];

	len = sizeof(ciphers);
	err = getsockopt(sock, SOL_TLS, TLS_CIPHERSUITE_LIST, ciphers, &len);
	if (err) {
		LOG_ERR("Error getting cipherlist: %d", errno);
	} else {
		int count = len / sizeof(int);

		LOG_INF("New cipherlist:");
		for (int i = 0; i < count; i++) {
			LOG_INF("%d. 0x%04X = %s", i, (unsigned int)ciphers[i],
			       IS_ENABLED(CONFIG_MBEDTLS) ?
				mbedtls_ssl_get_ciphersuite_name(ciphers[i]) : "");
		}
	}
#endif

	if (mfw_has_cid) {
		int cid_option = TLS_DTLS_CID_SUPPORTED;

		LOG_INF("  Enable connection id");
		err = setsockopt(sock, SOL_TLS, TLS_DTLS_CID, &cid_option, sizeof(cid_option));
		if (!err) {
		} else if (err != EOPNOTSUPP) {
			LOG_ERR("Error enabling connection ID: %d", errno);
			mfw_has_cid = false;
		} else {
			LOG_INF("Connection ID not supported by the installed modem firmware");
			mfw_has_cid = false;
		}

		int timeout = TLS_DTLS_HANDSHAKE_TIMEO_123S;

		LOG_INF("  Set handshake timeout %d", timeout);
		err = setsockopt(sock, SOL_TLS, TLS_DTLS_HANDSHAKE_TIMEO,
				 &timeout, sizeof(timeout));
		if (!err) {
		} else if (err != EOPNOTSUPP) {
			LOG_ERR("Error setting handshake timeout: %d", errno);
			mfw_has_cid = false;
		} else {
			mfw_has_cid = false;
		}
	}

	enum {
		NONE = 0,
		OPTIONAL = 1,
		REQUIRED = 2,
	};

	int verify = REQUIRED;

	LOG_INF("  Peer verify: %d", verify);
	err = setsockopt(sock, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
	if (err) {
		LOG_ERR("Failed to setup peer verification, errno %d", errno);
		return -errno;
	}

	return err;
}

bool dtls_cid_is_available(void)
{
	return mfw_has_cid;
}

int dtls_session_save(int sock)
{
	int dummy = 0;
	int err;

	LOG_DBG("Save DTLS CID session");
	err = setsockopt(sock, SOL_TLS, TLS_DTLS_CONN_SAVE, &dummy, sizeof(dummy));
	if (err) {
		LOG_DBG("Failed to save DTLS CID session, errno %d", errno);
	}
	return err;
}

int dtls_session_load(int sock)
{
	int dummy = 0;
	int err;

	LOG_DBG("Load DTLS CID session");
	err = setsockopt(sock, SOL_TLS, TLS_DTLS_CONN_LOAD, &dummy, sizeof(dummy));
	if (err) {
		LOG_DBG("Failed to load DTLS CID session, errno %d", errno);
	}
	return err;
}

bool dtls_cid_is_active(int sock)
{
	int err = 0;

	if (dtls_cid_active) {
		return true;
	}

	if (!mfw_has_cid) {
		return false;
	}

	int status = 0;
	int len = sizeof(status);

	err = getsockopt(sock, SOL_TLS, TLS_DTLS_HANDSHAKE_STATUS, &status, &len);
	if (!err) {
		if (len > 0) {
			if (status == TLS_DTLS_HANDSHAKE_STATUS_FULL) {
				LOG_INF("Full DTLS handshake performed");
			} else if (status == TLS_DTLS_HANDSHAKE_STATUS_CACHED) {
				LOG_INF("Cached DTLS handshake performed");
			} else {
				LOG_WRN("Unknown DTLS handshake status: %d", status);
			}
		} else {
			LOG_WRN("No DTLS status provided");
		}
	} else if (err != EOPNOTSUPP) {
		LOG_ERR("Error retrieving handshake status: %d", errno);
	} /* else the current modem firmware does not support this feature */

	len = sizeof(status);
	err = getsockopt(sock, SOL_TLS, TLS_DTLS_CID_STATUS, &status, &len);
	if (!err) {
		if (len > 0) {
			switch (status) {
			case TLS_DTLS_CID_STATUS_DISABLED:
				dtls_cid_active = false;
				LOG_INF("No DTLS CID used");
				break;
			case TLS_DTLS_CID_STATUS_DOWNLINK:
				dtls_cid_active = false;
				LOG_INF("DTLS CID downlink");
				break;
			case TLS_DTLS_CID_STATUS_UPLINK:
				dtls_cid_active = true;
				LOG_INF("DTLS CID uplink");
				break;
			case TLS_DTLS_CID_STATUS_BIDIRECTIONAL:
				dtls_cid_active = true;
				LOG_INF("DTLS CID bidirectional");
				break;
			default:
				LOG_WRN("Unknown DTLS CID status: %d", status);
				break;
			}
		} else {
			LOG_WRN("No DTLS CID status provided");
		}
	} else {
		LOG_ERR("Error retrieving DTLS CID status: %d", errno);
	}

	len = sizeof(status);
	err = getsockopt(sock, SOL_TLS, TLS_DTLS_CID, &status, &len);
	if (!err) {
		if (status == TLS_DTLS_CID_DISABLED) {
			dtls_cid_active = false;
		}
		if (len > 0) {
			LOG_INF("DTLS CID: %d", status);
		} else {
			LOG_WRN("No DTLS CID provided");
		}
	} else {
		LOG_ERR("Error retrieving DTLS CID: %d", errno);
	}

	return dtls_cid_active;
}
