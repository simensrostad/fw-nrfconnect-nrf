#include "cloud/cloud_wrapper.h"
#include <zephyr.h>
#include <net/lwm2m.h>
#include <modem/at_cmd.h>
#include <lwm2m_resource_ids.h>
#include <lwm2m_rd_client.h>
#include <date_time.h>

#define MODULE aws_iot_integration

#include <logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_CLOUD_INTEGRATION_LOG_LEVEL);

#if !defined(CONFIG_CLOUD_CLIENT_ID_USE_CUSTOM)
#define LWM2M_INTEGRATION_CLIENT_ID_LEN 15
#else
#define LWM2M_INTEGRATION_CLIENT_ID_LEN (sizeof(CONFIG_CLOUD_CLIENT_ID) - 1)
#endif

static cloud_wrap_evt_handler_t wrapper_evt_handler;

#define SERIAL_NUMBER_RID 2

/* LWM2M client instance */
static struct lwm2m_ctx client;

/* Unique endpoint name */
char endpoint_name[100];
static char client_id_buf[LWM2M_INTEGRATION_CLIENT_ID_LEN + 1];

/* Configurations */
#define SERVER_ADDR CONFIG_LWM2M_INTEGRATION_ENDPOINT_NAME
#define ENDPOINT_PREFIX CONFIG_LWM2M_INTEGRATION_ENDPOINT_PREFIX
#define SERVER_TLS_TAG CONFIG_LWM2M_INTEGRATION_TLS_TAG
#define BOOTSTRAP_TLS_TAG CONFIG_LWM2M_INTEGRATION_BOOTSTRAP_TLS_TAG
static char client_psk[] = CONFIG_LWM2M_INTEGRATION_PSK;

/* Location RIDs */
#define LATITUDE_RID 0
#define LONGITUDE_RID 1
#define ALTITUDE_RID 2
#define LOCATION_RADIUS_RID 3
#define LOCATION_VELOCITY_RID 4
#define LOCATION_TIMESTAMP_RID 5
#define LOCATION_SPEED_RID 6

/* LTE-FDD bearer & NB-IoT bearer */
#define LTE_FDD_BEARER 6U
#define NB_IOT_BEARER 7U

static uint8_t bearers[2] = { LTE_FDD_BEARER, NB_IOT_BEARER };

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
	imei_buf[LWM2M_INTEGRATION_CLIENT_ID_LEN] = 0;

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

	wrapper_evt_handler = event_handler;

	return 0;
}

int cloud_wrap_connect(void)
{
	lwm2m_rd_client_start(&client, endpoint_name, flags, rd_client_event, NULL);

	return 0;
}

int cloud_wrap_disconnect(void)
{
	lwm2m_rd_client_stop(&client, rd_client_event, NULL);

	return 0;
}

int cloud_wrap_state_get(void)
{
	return -ENOTSUP;
}

int cloud_wrap_config_send(struct cloud_data_cfg *config)
{
	return -ENOTSUP;
}

int cloud_wrap_data_send(void)
{
	/* Check return codes all APIs. */
	int err;
	struct cloud_data_modem_dynamic modem_dynamic = { 0 };
	struct cloud_data_modem_static modem_static = { 0 };
	struct cloud_data_gnss gnss = { 0 };
	struct cloud_data_ui ui = { 0 };
	struct cloud_data_accelerometer accelerometer = { 0 };
	struct cloud_data_sensors sensors = { 0 };
	struct cloud_data_battery battery = { 0 };
	struct cloud_data_neighbor_cells ncell = { 0 };

	cloud_codec_retrieve_modem_dynamic_buffer(&modem_dynamic);
	cloud_codec_retrieve_modem_static_buffer(&modem_static);
	cloud_codec_retrieve_gnss_buffer(&gnss);
	cloud_codec_retrieve_ui_buffer(&ui);
	cloud_codec_retrieve_accelerometer_buffer(&accelerometer);
	cloud_codec_retrieve_sensors_buffer(&sensors);
	cloud_codec_retrieve_battery_buffer(&battery);
	cloud_codec_retrieve_neighbor_cell_buffer(&ncell);


	/* GPS PVT */
	if (gnss.queued) {
		err = date_time_uptime_to_unix_time_ms(&gnss.gnss_ts);
		if (err) {
			LOG_ERR("date_time_uptime_to_unix_time_ms, error: %d", err);
			return err;
		}

		lwm2m_engine_set_float(LWM2M_PATH(LWM2M_OBJECT_LOCATION_ID, 0, LATITUDE_RID),
					 (double *)&gnss.pvt.lat);
		lwm2m_engine_set_float(LWM2M_PATH(LWM2M_OBJECT_LOCATION_ID, 0, LONGITUDE_RID),
					 (double *)&gnss.pvt.longi);
		lwm2m_engine_set_float(LWM2M_PATH(LWM2M_OBJECT_LOCATION_ID, 0, ALTITUDE_RID),
					 (double *)&gnss.pvt.alt);
		lwm2m_engine_set_float(LWM2M_PATH(LWM2M_OBJECT_LOCATION_ID, 0, LOCATION_RADIUS_RID),
					 (double *)&gnss.pvt.acc);
		lwm2m_engine_set_float(LWM2M_PATH(LWM2M_OBJECT_LOCATION_ID, 0, LOCATION_SPEED_RID),
					 (double *)&gnss.pvt.spd);
		lwm2m_engine_set_s64(LWM2M_PATH(LWM2M_OBJECT_LOCATION_ID, 0, LOCATION_TIMESTAMP_RID),
					 gnss.gnss_ts);

		gnss.queued = false;
	}

	if (modem_dynamic.queued) {
		LOG_WRN("Setting modem dynamic data");

		if (modem_dynamic.nw_mode == LTE_LC_LTE_MODE_LTEM) {
			lwm2m_engine_set_u8("4/0/0", LTE_FDD_BEARER);
		} else if (modem_dynamic.nw_mode == LTE_LC_LTE_MODE_NBIOT) {
			lwm2m_engine_set_u8("4/0/0", NB_IOT_BEARER);
		} else {
			LOG_WRN("No network bearer set");
		}

		lwm2m_engine_create_res_inst("4/0/1/0");
		lwm2m_engine_set_res_data("4/0/1/0", &bearers[0], sizeof(bearers[0]),
					  LWM2M_RES_DATA_FLAG_RO);

		lwm2m_engine_create_res_inst("4/0/1/1");
		lwm2m_engine_set_res_data("4/0/1/1", &bearers[1], sizeof(bearers[1]),
					  LWM2M_RES_DATA_FLAG_RO);

		lwm2m_engine_create_res_inst("4/0/4/0");
		lwm2m_engine_set_res_data("4/0/4/0", modem_dynamic.ip, sizeof(modem_dynamic.ip), LWM2M_RES_DATA_FLAG_RO);

		lwm2m_engine_set_s8("4/0/2", modem_dynamic.rsrp);
		lwm2m_engine_set_u32("4/0/8", modem_dynamic.cell);
		lwm2m_engine_set_u16("4/0/9", modem_dynamic.mnc);
		lwm2m_engine_set_u16("4/0/10", modem_dynamic.mcc);

		modem_dynamic.queued = false;
	}

	char *path_list[2] = {
		[0] = "4",
		[1] = "6"
	};

	/* Trigger update of objects */
	lwm2m_engine_send(&client, path_list, ARRAY_SIZE(path_list));

	return 0;
}

int cloud_wrap_batch_send(void)
{
	return -ENOTSUP;
}

int cloud_wrap_ui_send(void)
{
	return -ENOTSUP;
}

int cloud_wrap_neighbor_cells_send(void)
{
	return -ENOTSUP;
}

int cloud_wrap_agps_request_send(struct cloud_data_agps_request *request)
{
	return -ENOTSUP;
}

int cloud_wrap_pgps_request_send(struct cloud_data_pgps_request *request)
{
	return -ENOTSUP;
}

int cloud_wrap_memfault_data_send(char *buf, size_t len)
{
	return -ENOTSUP;
}
