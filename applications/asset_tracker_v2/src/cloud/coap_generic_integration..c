#include <zephyr.h>
#include <net/coap_generic.h>
#include <modem/at_cmd.h>

#include "cloud/cloud_wrapper.h"

#define MODULE coap_generic_integration

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_CLOUD_INTEGRATION_LOG_LEVEL);

#if !defined(CONFIG_CLOUD_CLIENT_ID_USE_CUSTOM)
#define CLIENT_ID_LEN 15
#else
#define CLIENT_ID_LEN (sizeof(CONFIG_CLOUD_CLIENT_ID) - 1)
#endif

static char client_id_buf[CLIENT_ID_LEN + 1];
static struct coap_generic_config config;
static cloud_wrap_evt_handler_t wrapper_evt_handler;

static void cloud_wrapper_notify_event(const struct cloud_wrap_event *evt)
{
	if ((wrapper_evt_handler != NULL) && (evt != NULL)) {
		wrapper_evt_handler(evt);
	} else {
		LOG_ERR("Library event handler not registered, or empty event");
	}
}

static void coap_generic_event_handler(struct coap_generic_evt *const evt)
{
	struct cloud_wrap_event cloud_wrap_evt = { 0 };
	bool notify = false;

	switch (evt->type) {
	case COAP_GENERIC_EVT_CONNECTING:
		LOG_DBG("COAP_GENERIC_EVT_CONNECTING");
		cloud_wrap_evt.type = CLOUD_WRAP_EVT_CONNECTING;
		notify = true;
		break;
	case COAP_GENERIC_EVT_CONNECTED:
		LOG_DBG("COAP_GENERIC_EVT_CONNECTED");
		break;
	case COAP_GENERIC_EVT_CONNECTION_FAILED:
		LOG_DBG("COAP_GENERIC_EVT_CONNECTION_FAILED");
		break;
	case COAP_GENERIC_EVT_DISCONNECTED:
		LOG_DBG("COAP_GENERIC_EVT_DISCONNECTED");
		cloud_wrap_evt.type = CLOUD_WRAP_EVT_DISCONNECTED;
		notify = true;
		break;
	case COAP_GENERIC_EVT_READY:
		LOG_DBG("COAP_GENERIC_EVT_READY");
		cloud_wrap_evt.type = CLOUD_WRAP_EVT_CONNECTED;
		notify = true;
		break;
	case COAP_GENERIC_EVT_DATA_RECEIVED:
		LOG_DBG("COAP_GENERIC_EVT_DATA_RECEIVED");
		cloud_wrap_evt.type = CLOUD_WRAP_EVT_DATA_RECEIVED;
		cloud_wrap_evt.data.buf = evt->data.msg.ptr;
		cloud_wrap_evt.data.len = evt->data.msg.len;
		notify = true;
		break;
	case COAP_GENERIC_EVT_STATE_RECEIVED:
		LOG_DBG("COAP_GENERIC_EVT_STATE_RECEIVED");
		cloud_wrap_evt.type = CLOUD_WRAP_EVT_DATA_RECEIVED;
		cloud_wrap_evt.data.buf = evt->data.msg.ptr;
		cloud_wrap_evt.data.len = evt->data.msg.len;
		notify = true;
		break;
	case COAP_GENERIC_EVT_PUBACK:
		LOG_DBG("COAP_GENERIC_EVT_PUBACK");
		break;
	case COAP_GENERIC_EVT_ERROR:
		LOG_DBG("COAP_GENERIC_EVT_ERROR");
		cloud_wrap_evt.type = CLOUD_WRAP_EVT_ERROR;
		cloud_wrap_evt.err = evt->data.err;
		notify = true;
		break;
	case COAP_GENERIC_EVT_FOTA_START:
		LOG_DBG("COAP_GENERIC_EVT_FOTA_START");
		cloud_wrap_evt.type = CLOUD_WRAP_EVT_FOTA_START;
		notify = true;
		break;
	case COAP_GENERIC_EVT_FOTA_DONE:
		LOG_DBG("COAP_GENERIC_EVT_FOTA_DONE");
		cloud_wrap_evt.type = CLOUD_WRAP_EVT_FOTA_DONE;
		notify = true;
		break;
	case COAP_GENERIC_EVT_FOTA_ERASE_PENDING:
		LOG_DBG("COAP_GENERIC_EVT_FOTA_ERASE_PENDING");
		cloud_wrap_evt.type = CLOUD_WRAP_EVT_FOTA_ERASE_PENDING;
		notify = true;
		break;
	case COAP_GENERIC_EVT_FOTA_ERASE_DONE:
		LOG_DBG("COAP_GENERIC_EVT_FOTA_ERASE_DONE");
		cloud_wrap_evt.type = CLOUD_WRAP_EVT_FOTA_ERASE_DONE;
		notify = true;
		break;
	case COAP_GENERIC_EVT_FOTA_ERROR:
		LOG_DBG("COAP_GENERIC_EVT_FOTA_ERROR");
		cloud_wrap_evt.type = CLOUD_WRAP_EVT_FOTA_ERROR;
		notify = true;
		break;
	default:
		LOG_ERR("Unknown Coap Generic backend event type: %d", evt->type);
		break;
	}

	if (notify) {
		cloud_wrapper_notify_event(&cloud_wrap_evt);
	}
}

int cloud_wrap_init(cloud_wrap_evt_handler_t event_handler)
{
	int err;

#if !defined(CONFIG_CLOUD_CLIENT_ID_USE_CUSTOM)
	char imei_buf[20];

	/* Retrieve device IMEI from modem. */
	err = at_cmd_write("AT+CGSN", imei_buf, sizeof(imei_buf), NULL);
	if (err) {
		LOG_ERR("Not able to retrieve device IMEI from modem");
		return err;
	}

	/* Set null character at the end of the device IMEI. */
	imei_buf[CLIENT_ID_LEN] = 0;

	snprintk(client_id_buf, sizeof(client_id_buf), "%s", imei_buf);

#else
	snprintk(client_id_buf, sizeof(client_id_buf), "%s", CONFIG_CLOUD_CLIENT_ID);
#endif

	/* Fetch IMEI from modem data and set IMEI as cloud connection ID */
	config.device_id = client_id_buf;
	config.device_id_len = strlen(client_id_buf);

	err = coap_generic_init(&config, coap_generic_event_handler);
	if (err) {
		LOG_ERR("coap_generic_init, error: %d", err);
		return err;
	}

	LOG_DBG("********************************************");
	LOG_DBG(" The Asset Tracker v2 has started");
	LOG_DBG(" Version:      %s", CONFIG_ASSET_TRACKER_V2_APP_VERSION);
	LOG_DBG(" Client ID:    %s", log_strdup(client_id_buf));
	LOG_DBG(" DPS endpoint: %s", CONFIG_COAP_GENERIC_HOSTNAME);
	LOG_DBG("********************************************");

	wrapper_evt_handler = event_handler;

	return 0;
}

int cloud_wrap_connect(void)
{
	int err;

	err = coap_generic_connect();
	if (err) {
		LOG_ERR("coap_generic_connect, error: %d", err);
		return err;
	}

	return 0;
}

int cloud_wrap_disconnect(void)
{
	int err;

	err = coap_generic_disconnect();
	if (err) {
		LOG_ERR("coap_generic_disconnect, error: %d", err);
		return err;
	}

	return 0;
}

int cloud_wrap_state_get(void)
{
	int err;
	struct coap_generic_data msg = {
		.ptr = REQUEST_DEVICE_STATE_STRING,
		.len = sizeof(REQUEST_DEVICE_STATE_STRING) - 1,
		.qos = COAP_QOS_1_CONFIRMABLE,
		.resource.type = COAP_GENERIC_RESOURCE_STATE
	};

	err = coap_generic_send(&msg);
	if (err) {
		LOG_ERR("coap_generic_send, error: %d", err);
		return err;
	}

	return 0;
}

int cloud_wrap_state_send(char *buf, size_t len)
{
	int err;
	struct coap_generic_data msg = {
		.ptr = buf,
		.len = len,
		.qos = COAP_QOS_0_NON_CONFIRMABLE,
		.resource.type = COAP_GENERIC_RESOURCE_STATE,
	};

	err = coap_generic_send(&msg);
	if (err) {
		LOG_ERR("coap_generic_send, error: %d", err);
		return err;
	}

	return 0;
}

int cloud_wrap_data_send(char *buf, size_t len)
{
	int err;
	struct coap_generic_data msg = {
		.ptr = buf,
		.len = len,
		.qos = COAP_QOS_0_NON_CONFIRMABLE,
		.resource.type = COAP_GENERIC_RESOURCE_STATE,
	};

	err = coap_generic_send(&msg);
	if (err) {
		LOG_ERR("coap_generic_send, error: %d", err);
		return err;
	}

	return 0;
}

int cloud_wrap_batch_send(char *buf, size_t len)
{
	int err;
	struct coap_generic_data msg = {
		.ptr = buf,
		.len = len,
		.qos = COAP_QOS_NON_CONFIRMABLE,
		.resource.type = COAP_GENERIC_RESOURCE_BATCH,
	};

	err = coap_generic_send(&msg);
	if (err) {
		LOG_ERR("coap_generic_send, error: %d", err);
		return err;
	}

	return 0;
}

int cloud_wrap_ui_send(char *buf, size_t len)
{
	int err;
	struct coap_generic_data msg = {
		.ptr = buf,
		.len = len,
		.qos = COAP_QOS_NON_CONFIRMABLE,
		.resource.type = COAP_GENERIC_RESOURCE_MESSAGE
	};

	err = coap_generic_send(&msg);
	if (err) {
		LOG_ERR("coap_generic_send, error: %d", err);
		return err;
	}

	return 0;
}
