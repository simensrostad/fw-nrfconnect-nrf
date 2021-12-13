#include "cloud/cloud_wrapper.h"
#include <zephyr.h>
#include <net/lwm2m.h>
#include <modem/at_cmd.h>

#define MODULE aws_iot_integration

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_CLOUD_INTEGRATION_LOG_LEVEL);

#if !defined(CONFIG_CLOUD_CLIENT_ID_USE_CUSTOM)
#define AVSYSTEM_CLIENT_ID_LEN 15
#else
#define AVSYSTEM_CLIENT_ID_LEN (sizeof(CONFIG_CLOUD_CLIENT_ID) - 1)
#endif

static cloud_wrap_evt_handler_t wrapper_evt_handler;

#define SERIAL_NUMBER_RID 2

/* LWM2M client instance */
static struct lwm2m_ctx client;

/* Unique endpoint name */
char endpoint_name[100];
static char client_id_buf[AVSYSTEM_CLIENT_ID_LEN + 1];

/* Configurations */

static void cloud_wrapper_notify_event(const struct cloud_wrap_event *evt)
{
	if ((wrapper_evt_handler != NULL) && (evt != NULL)) {
		wrapper_evt_handler(evt);
	} else {
		LOG_ERR("Library event handler not registered, or empty event");
	}
}

static int load_credentials_dummy(struct lwm2m_ctx *client_ctx)
{
	return 0;
}

int lwm2m_init_security(struct lwm2m_ctx *ctx, char *endpoint)
{
	int ret;
	char *server_url;
	uint16_t server_url_len;
	uint8_t server_url_flags;

	/* setup SECURITY object */

	ret = lwm2m_engine_get_res_data("0/0/0", (void **)&server_url, &server_url_len,
					&server_url_flags);
	if (ret < 0) {
		return ret;
	}

	snprintk(server_url, server_url_len, "coap%s//%s%s%s",
		 IS_ENABLED(CONFIG_LWM2M_DTLS_SUPPORT) ? "s:" : ":",
		 strchr(SERVER_ADDR, ':') ? "[" : "", SERVER_ADDR,
		 strchr(SERVER_ADDR, ':') ? "]" : "");

	LOG_WRN("Server URL: %s", server_url);
	LOG_WRN("Endpoint: %s", endpoint);

	/* Security Mode */
	lwm2m_engine_set_u8("0/0/2", IS_ENABLED(CONFIG_LWM2M_DTLS_SUPPORT) ? 0 : 3);
#if defined(CONFIG_LWM2M_DTLS_SUPPORT)
	ctx->tls_tag = IS_ENABLED(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP) ? BOOTSTRAP_TLS_TAG :
										    SERVER_TLS_TAG;
	ctx->load_credentials = load_credentials_dummy;

	lwm2m_engine_set_string("0/0/3", endpoint);
	lwm2m_engine_set_opaque("0/0/5", (void *)client_psk, sizeof(client_psk));
#endif /* CONFIG_LWM2M_DTLS_SUPPORT */

#if defined(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP)
	/* Mark 1st instance of security object as a bootstrap server */
	lwm2m_engine_set_u8("0/0/1", 1);
#else
	/* Security and Server object need matching Short Server ID value. */
	lwm2m_engine_set_u16("0/0/10", 101);
	lwm2m_engine_set_u16("1/0/0", 101);
#endif

	return ret;
}

static void rd_client_event(struct lwm2m_ctx *client, enum lwm2m_rd_client_event client_event)
{
	struct cloud_wrap_event cloud_wrap_evt = { 0 };
	bool notify = false;

	switch (client_event) {
	case LWM2M_RD_CLIENT_EVENT_NONE:
		LOG_WRN("LWM2M_RD_CLIENT_EVENT_NONE");
		break;
	case LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_REG_FAILURE:
		LOG_WRN("LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_REG_FAILURE");
		break;
	case LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_REG_COMPLETE:
		LOG_WRN("LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_REG_COMPLETE");
		break;
	case LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_TRANSFER_COMPLETE:
		LOG_WRN("LWM2M_RD_CLIENT_EVENT_BOOTSTRAP_TRANSFER_COMPLETE");
		LOG_WRN("Boostrap finished, provisioning credentials.");
		// provision_credentials();
		break;
	case LWM2M_RD_CLIENT_EVENT_REGISTRATION_FAILURE:
		LOG_WRN("LWM2M_RD_CLIENT_EVENT_REGISTRATION_FAILURE");
		break;
	case LWM2M_RD_CLIENT_EVENT_REGISTRATION_COMPLETE:
		LOG_WRN("LWM2M_RD_CLIENT_EVENT_REGISTRATION_COMPLETE");
		cloud_wrap_evt.type = CLOUD_WRAP_EVT_CONNECTED;
		notify = true;
		break;
	case LWM2M_RD_CLIENT_EVENT_REG_UPDATE_FAILURE:
		LOG_WRN("LWM2M_RD_CLIENT_EVENT_REG_UPDATE_FAILURE");
		break;
	case LWM2M_RD_CLIENT_EVENT_REG_UPDATE_COMPLETE:
		LOG_WRN("LWM2M_RD_CLIENT_EVENT_REG_UPDATE_COMPLETE");
		break;
	case LWM2M_RD_CLIENT_EVENT_DEREGISTER_FAILURE:
		LOG_WRN("LWM2M_RD_CLIENT_EVENT_DEREGISTER_FAILURE");
		break;
	case LWM2M_RD_CLIENT_EVENT_DISCONNECT:
		LOG_WRN("LWM2M_RD_CLIENT_EVENT_DISCONNECT");
		cloud_wrap_evt.type = CLOUD_WRAP_EVT_DISCONNECTED;
		notify = true;
		break;
	case LWM2M_RD_CLIENT_EVENT_QUEUE_MODE_RX_OFF:
		LOG_WRN("LWM2M_RD_CLIENT_EVENT_QUEUE_MODE_RX_OFF");
		break;
	case LWM2M_RD_CLIENT_EVENT_NETWORK_ERROR:
		LOG_ERR("LWM2M_RD_CLIENT_EVENT_NETWORK_ERROR");
		break;
	default:
		LOG_WRN("event?");
		break;
	}

	if (notify) {
		cloud_wrapper_notify_event(&cloud_wrap_evt);
	}
}

uint32_t flags = IS_ENABLED(CONFIG_LWM2M_RD_CLIENT_SUPPORT_BOOTSTRAP) ? LWM2M_RD_CLIENT_FLAG_BOOTSTRAP : 0;

int cloud_wrap_init(cloud_wrap_evt_handler_t event_handler)
{
	int err, ret;

#if !defined(CONFIG_CLOUD_CLIENT_ID_USE_CUSTOM)
	char imei_buf[20];

	/* Retrieve device IMEI from modem. */
	err = at_cmd_write("AT+CGSN", imei_buf, sizeof(imei_buf), NULL);
	if (err) {
		LOG_ERR("Not able to retrieve device IMEI from modem");
		return err;
	}

	/* Set null character at the end of the device IMEI. */
	imei_buf[AVSYSTEM_CLIENT_ID_LEN] = 0;

	strncpy(client_id_buf, imei_buf, sizeof(client_id_buf) - 1);

#else
	snprintf(client_id_buf, sizeof(client_id_buf), "%s",
		 CONFIG_CLOUD_CLIENT_ID);
#endif

	snprintk(endpoint_name, sizeof(endpoint_name), "%s%s", ENDPOINT_PREFIX, client_id_buf);
	LOG_INF("Endpoint name: %s", log_strdup(endpoint_name));

	/* Clear LwM2M client instance */
	(void)memset(&client, 0x0, sizeof(client));

	/* Set IMEI number */
	lwm2m_engine_set_res_data(LWM2M_PATH(LWM2M_OBJECT_DEVICE_ID, 0, SERIAL_NUMBER_RID),
				  client_id_buf, strlen(client_id_buf), LWM2M_RES_DATA_FLAG_RO);

	/* Setup lwm2m client with PSK */
	err = lwm2m_init_security(&client, endpoint_name);
	if (err) {
		LOG_ERR("lwm2m_init_security");
		return err;
	}

	/* ________________________________________ */

	/* Setup objects supported by the application.
	 * Setup context and reference to correct credentials.
	 */

	/* ________________________________________ */

	wrapper_evt_handler = event_handler;

	return 0;
}

int cloud_wrap_connect(void)
{
	lwm2m_rd_client_start(&client, endpoint_name, flags, rd_client_event);

	return 0;
}

int cloud_wrap_disconnect(void)
{
	lwm2m_rd_client_stop(&client, rd_client_event);

	return 0;
}

int cloud_wrap_state_get(void)
{
	return -ENOTSUP;
}

int cloud_wrap_state_send(char *buf, size_t len)
{
	return -ENOTSUP;
}

int cloud_wrap_data_send(char *buf, size_t len)
{
	return -ENOTSUP;
}

int cloud_wrap_batch_send(char *buf, size_t len)
{
	return -ENOTSUP;
}

int cloud_wrap_ui_send(char *buf, size_t len)
{
	return -ENOTSUP;
}

int cloud_wrap_neighbor_cells_send(char *buf, size_t len)
{
	return -ENOTSUP;
}

int cloud_wrap_agps_request_send(char *buf, size_t len)
{
	return -ENOTSUP;
}

int cloud_wrap_pgps_request_send(char *buf, size_t len)
{
	return -ENOTSUP;
}

int cloud_wrap_memfault_data_send(char *buf, size_t len)
{
	return -ENOTSUP;
}
