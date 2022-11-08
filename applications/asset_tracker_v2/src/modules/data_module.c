/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <app_event_manager.h>
#include <zephyr/settings/settings.h>
#include <date_time.h>
#include <modem/modem_info.h>
#if defined(CONFIG_NRF_CLOUD_AGPS)
#include <net/nrf_cloud_agps.h>
#endif

#if defined(CONFIG_NRF_CLOUD_PGPS)
#include <net/nrf_cloud_pgps.h>
#endif

#include "codec/lwm2m_codec.h"

#define MODULE data_module

#include "modules_common.h"
#include "events/app_module_event.h"
#include "events/cloud_module_event.h"
#include "events/data_module_event.h"
#include "events/gnss_module_event.h"
#include "events/modem_module_event.h"
#include "events/sensor_module_event.h"
#include "events/ui_module_event.h"
#include "events/util_module_event.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DATA_MODULE_LOG_LEVEL);

#define DEVICE_SETTINGS_KEY			"data_module"
#define DEVICE_SETTINGS_CONFIG_KEY		"config"

struct data_msg_data {
	union {
		struct modem_module_event modem;
		struct cloud_module_event cloud;
		struct gnss_module_event gnss;
		struct ui_module_event ui;
		struct sensor_module_event sensor;
		struct data_module_event data;
		struct app_module_event app;
		struct util_module_event util;
	} module;
};

/* Data module super states. */
static enum state_type {
	STATE_CLOUD_DISCONNECTED,
	STATE_CLOUD_CONNECTED,
	STATE_SHUTDOWN
} state;

/* */
static struct cloud_data_gnss gnss_buf;
static struct cloud_data_sensors sensors_buf;
static struct cloud_data_ui ui_buf;
static struct cloud_data_impact impact_buf;
static struct cloud_data_battery bat_buf;
static struct cloud_data_modem_dynamic modem_dyn_buf;
static struct cloud_data_neighbor_cells neighbor_cells;
static struct cloud_data_modem_static modem_stat;

static K_SEM_DEFINE(config_load_sem, 0, 1);

/* Default device configuration. */
static struct cloud_data_cfg current_cfg = {
	.gnss_timeout		 = CONFIG_DATA_GNSS_TIMEOUT_SECONDS,
	.active_mode		 = IS_ENABLED(CONFIG_DATA_DEVICE_MODE_ACTIVE),
	.active_wait_timeout	 = CONFIG_DATA_ACTIVE_TIMEOUT_SECONDS,
	.movement_resolution	 = CONFIG_DATA_MOVEMENT_RESOLUTION_SECONDS,
	.movement_timeout	 = CONFIG_DATA_MOVEMENT_TIMEOUT_SECONDS,
	.accelerometer_activity_threshold	= CONFIG_DATA_ACCELEROMETER_ACT_THRESHOLD,
	.accelerometer_inactivity_threshold	= CONFIG_DATA_ACCELEROMETER_INACT_THRESHOLD,
	.accelerometer_inactivity_timeout	= CONFIG_DATA_ACCELEROMETER_INACT_TIMEOUT_SECONDS,
	.no_data.gnss		 = !IS_ENABLED(CONFIG_DATA_SAMPLE_GNSS_DEFAULT),
	.no_data.neighbor_cell	 = !IS_ENABLED(CONFIG_DATA_SAMPLE_NEIGHBOR_CELLS_DEFAULT)
};

static struct k_work_delayable data_send_work;

/* List used to keep track of responses from other modules with data that is
 * requested to be sampled/published.
 */
static enum app_module_data_type req_type_list[APP_DATA_COUNT];

/* Total number of data types requested for a particular sample/publish
 * cycle.
 */
static int recv_req_data_count;

/* Counter of data types received from other modules. When this number
 * matches the affirmed_data_type variable all requested data has been
 * received by the Data module.
 */
static int req_data_count;

/* List of data types that are supported to be sent based on LTE connection evaluation. */
enum coneval_supported_data_type {
	UNUSED,
	GENERIC,
	BATCH,
	NEIGHBOR_CELLS,
	COUNT,
};

/* Data module message queue. */
#define DATA_QUEUE_ENTRY_COUNT		10
#define DATA_QUEUE_BYTE_ALIGNMENT	4

K_MSGQ_DEFINE(msgq_data, sizeof(struct data_msg_data),
	      DATA_QUEUE_ENTRY_COUNT, DATA_QUEUE_BYTE_ALIGNMENT);

static struct module_data self = {
	.name = "data",
	.msg_q = &msgq_data,
	.supports_shutdown = true,
};

/* Forward declarations */
static void data_send_work_fn(struct k_work *work);
static int config_settings_handler(const char *key, size_t len,
				   settings_read_cb read_cb, void *cb_arg);
static void new_config_handle(struct cloud_data_cfg *new_config);

/* Static handlers */
SETTINGS_STATIC_HANDLER_DEFINE(MODULE, DEVICE_SETTINGS_KEY, NULL,
			       config_settings_handler, NULL, NULL);

/* Convenience functions used in internal state handling. */
static char *state2str(enum state_type new_state)
{
	switch (new_state) {
	case STATE_CLOUD_DISCONNECTED:
		return "STATE_CLOUD_DISCONNECTED";
	case STATE_CLOUD_CONNECTED:
		return "STATE_CLOUD_CONNECTED";
	case STATE_SHUTDOWN:
		return "STATE_SHUTDOWN";
	default:
		return "Unknown";
	}
}

static void state_set(enum state_type new_state)
{
	if (new_state == state) {
		LOG_DBG("State: %s", state2str(state));
		return;
	}

	LOG_DBG("State transition %s --> %s",
		state2str(state),
		state2str(new_state));

	state = new_state;
}

/* Handlers */
static bool app_event_handler(const struct app_event_header *aeh)
{
	struct data_msg_data msg = {0};
	bool enqueue_msg = false;

	if (is_modem_module_event(aeh)) {
		struct modem_module_event *event = cast_modem_module_event(aeh);

		msg.module.modem = *event;
		enqueue_msg = true;
	}

	if (is_cloud_module_event(aeh)) {
		struct cloud_module_event *event = cast_cloud_module_event(aeh);

		msg.module.cloud = *event;
		enqueue_msg = true;
	}

	if (is_gnss_module_event(aeh)) {
		struct gnss_module_event *event = cast_gnss_module_event(aeh);

		msg.module.gnss = *event;
		enqueue_msg = true;
	}

	if (is_sensor_module_event(aeh)) {
		struct sensor_module_event *event =
				cast_sensor_module_event(aeh);

		msg.module.sensor = *event;
		enqueue_msg = true;
	}

	if (is_ui_module_event(aeh)) {
		struct ui_module_event *event = cast_ui_module_event(aeh);

		msg.module.ui = *event;
		enqueue_msg = true;
	}

	if (is_app_module_event(aeh)) {
		struct app_module_event *event = cast_app_module_event(aeh);

		msg.module.app = *event;
		enqueue_msg = true;
	}

	if (is_data_module_event(aeh)) {
		struct data_module_event *event = cast_data_module_event(aeh);

		msg.module.data = *event;
		enqueue_msg = true;
	}

	if (is_util_module_event(aeh)) {
		struct util_module_event *event = cast_util_module_event(aeh);

		msg.module.util = *event;
		enqueue_msg = true;
	}

	if (enqueue_msg) {
		int err = module_enqueue_msg(&self, &msg);

		if (err) {
			LOG_ERR("Message could not be enqueued");
			SEND_ERROR(data, DATA_EVT_ERROR, err);
		}
	}

	return false;
}

static int config_settings_handler(const char *key, size_t len,
				   settings_read_cb read_cb, void *cb_arg)
{
	int err = 0;

	if (strcmp(key, DEVICE_SETTINGS_CONFIG_KEY) == 0) {
		err = read_cb(cb_arg, &current_cfg, sizeof(current_cfg));
		if (err < 0) {
			LOG_ERR("Failed to load configuration, error: %d", err);
		} else {
			LOG_DBG("Device configuration loaded from flash");
			err = 0;
		}
	}

	k_sem_give(&config_load_sem);
	return err;
}

static void date_time_event_handler(const struct date_time_evt *evt)
{
	switch (evt->type) {
	case DATE_TIME_OBTAINED_MODEM:
		/* Fall through. */
	case DATE_TIME_OBTAINED_NTP:
		/* Fall through. */
	case DATE_TIME_OBTAINED_EXT: {
		SEND_EVENT(data, DATA_EVT_DATE_TIME_OBTAINED);

		/* De-register handler. At this point the application will have
		 * date time to depend on indefinitely until a reboot occurs.
		 */
		date_time_register_handler(NULL);
		break;
	}
	case DATE_TIME_NOT_OBTAINED:
		break;
	default:
		break;
	}
}

static int save_config(const void *buf, size_t buf_len)
{
	int err;

	err = settings_save_one(DEVICE_SETTINGS_KEY "/"
				DEVICE_SETTINGS_CONFIG_KEY,
				buf, buf_len);
	if (err) {
		LOG_WRN("settings_save_one, error: %d", err);
		return err;
	}

	LOG_DBG("Device configuration stored to flash");

	return 0;
}

static void cloud_codec_event_handler(const struct cloud_codec_evt *evt)
{
	if (evt->type == CLOUD_CODEC_EVT_CONFIG_UPDATE) {
		new_config_handle((struct cloud_data_cfg *)&evt->config_update);
	} else {
		LOG_ERR("Unknown cloud codec event.");
	}
}

static int setup(void)
{
	int err;

	err = settings_subsys_init();
	if (err) {
		LOG_ERR("settings_subsys_init, error: %d", err);
		return err;
	}

	err = settings_load_subtree(DEVICE_SETTINGS_KEY);
	if (err) {
		LOG_ERR("settings_load_subtree, error: %d", err);
		return err;
	}

	/* Wait up to 1 seconds for the settings API to load the device configuration stored
	 * to flash, if any.
	 */
	if (k_sem_take(&config_load_sem, K_SECONDS(1)) != 0) {
		LOG_DBG("Failed retrieveing the device configuration from flash in time");
	}

	err = cloud_codec_init(&current_cfg, cloud_codec_event_handler);
	if (err) {
		LOG_ERR("cloud_codec_init, error: %d", err);
		return err;
	}

	date_time_register_handler(date_time_event_handler);
	return 0;
}

static void config_print_all(void)
{
	if (current_cfg.active_mode) {
		LOG_DBG("Device mode: Active");
	} else {
		LOG_DBG("Device mode: Passive");
	}

	LOG_DBG("Active wait timeout: %d", current_cfg.active_wait_timeout);
	LOG_DBG("Movement resolution: %d", current_cfg.movement_resolution);
	LOG_DBG("Movement timeout: %d", current_cfg.movement_timeout);
	LOG_DBG("GPS timeout: %d", current_cfg.gnss_timeout);
	LOG_DBG("Accelerometer act threshold: %.2f",
		 current_cfg.accelerometer_activity_threshold);
	LOG_DBG("Accelerometer inact threshold: %.2f",
		 current_cfg.accelerometer_inactivity_threshold);
	LOG_DBG("Accelerometer inact timeout: %.2f",
		 current_cfg.accelerometer_inactivity_timeout);

	if (!current_cfg.no_data.neighbor_cell) {
		LOG_DBG("Requesting of neighbor cell data is enabled");
	} else {
		LOG_DBG("Requesting of neighbor cell data is disabled");
	}

	if (!current_cfg.no_data.gnss) {
		LOG_DBG("Requesting of GNSS data is enabled");
	} else {
		LOG_DBG("Requesting of GNSS data is disabled");
	}
}

static void config_distribute(enum data_module_event_type type)
{
	struct data_module_event *data_module_event = new_data_module_event();

	__ASSERT(data_module_event, "Not enough heap left to allocate event");

	data_module_event->type = type;
	data_module_event->data.cfg = current_cfg;

	APP_EVENT_SUBMIT(data_module_event);
}

static void data_send(enum data_module_event_type event,
		      struct cloud_codec_data *data)
{
	struct data_module_event *module_event = new_data_module_event();

	__ASSERT(module_event, "Not enough heap left to allocate event");

	module_event->type = event;

	BUILD_ASSERT((sizeof(data->paths) == sizeof(module_event->data.buffer.paths)),
			"Size of the object path list does not match");
	BUILD_ASSERT((sizeof(data->paths[0]) == sizeof(module_event->data.buffer.paths[0])),
			"Size of an entry in the object path list does not match");

	memcpy(module_event->data.buffer.paths, data->paths, sizeof(data->paths));
	module_event->data.buffer.valid_object_paths = data->valid_object_paths;

	APP_EVENT_SUBMIT(module_event);

	/* Reset buffer */
	memset(data, 0, sizeof(struct cloud_codec_data));
}

/* This function allocates buffer on the heap, which needs to be freed after use. */
static void data_encode(void)
{
	int err;
	struct cloud_codec_data codec = { 0 };

	if (!date_time_is_valid()) {
		/* Date time library does not have valid time to
		 * timestamp cloud data. Abort cloud publicaton. Data will
		 * be cached in it respective ringbuffer.
		 */
		return;
	}

	err = cloud_codec_encode_neighbor_cells(&codec, &neighbor_cells);
	switch (err) {
	case 0:
		LOG_DBG("Neighbor cell data encoded successfully");
		data_send(DATA_EVT_NEIGHBOR_CELLS_DATA_SEND, &codec);
		break;
	case -ENODATA:
		LOG_DBG("No neighbor cells data to encode, error: %d", err);
		break;
	default:
		LOG_ERR("Error encoding neighbor cells data: %d", err);
		SEND_ERROR(data, DATA_EVT_ERROR, err);
		return;
	}

	err = cloud_codec_encode_data(&codec,
				      &gnss_buf,
				      &sensors_buf,
				      &modem_stat,
				      &modem_dyn_buf,
				      &ui_buf,
				      &impact_buf,
				      &bat_buf);
	switch (err) {
	case 0:
		LOG_DBG("Data encoded successfully");
		data_send(DATA_EVT_DATA_SEND, &codec);
		break;
	case -ENODATA:
		/* This error might occur when data has not been obtained prior
			* to data encoding.
			*/
		LOG_DBG("No new data to encode");
		break;
	default:
		LOG_ERR("Error encoding message %d", err);
		SEND_ERROR(data, DATA_EVT_ERROR, err);
		return;
	}
}

#if defined(CONFIG_NRF_CLOUD_AGPS)
static int get_modem_info(struct modem_param_info *const modem_info)
{
	__ASSERT_NO_MSG(modem_info != NULL);

	int err = modem_info_init();

	if (err) {
		LOG_ERR("Could not initialize modem info module, error: %d", err);
		return err;
	}

	err = modem_info_params_init(modem_info);
	if (err) {
		LOG_ERR("Could not initialize modem info parameters, error: %d", err);
		return err;
	}

	err = modem_info_params_get(modem_info);
	if (err) {
		LOG_ERR("Could not obtain cell information, error: %d", err);
		return err;
	}

	return 0;
}

/**
 * @brief Combine and encode modem network parameters together with the incoming A-GPS data request
 *	  types to form the A-GPS request.
 *
 * @param[in] incoming_request Pointer to a structure containing A-GPS data types that has been
 *			       requested by the modem. If incoming_request is NULL, all A-GPS data
 *			       types are requested.
 *
 * @return 0 on success, otherwise a negative error code indicating reason of failure.
 */
static int agps_request_encode(struct nrf_modem_gnss_agps_data_frame *incoming_request)
{
	int err;
	struct cloud_codec_data codec = {0};
	static struct modem_param_info modem_info = {0};
	static struct cloud_data_agps_request cloud_agps_request = {0};

	err = get_modem_info(&modem_info);
	if (err) {
		return err;
	}

	if (incoming_request == NULL) {
		const uint32_t mask = IS_ENABLED(CONFIG_NRF_CLOUD_PGPS) ? 0u : 0xFFFFFFFFu;

		LOG_DBG("Requesting all A-GPS elements");
		cloud_agps_request.request.sv_mask_ephe = mask,
		cloud_agps_request.request.sv_mask_alm = mask,
		cloud_agps_request.request.data_flags =
					NRF_MODEM_GNSS_AGPS_GPS_UTC_REQUEST |
					NRF_MODEM_GNSS_AGPS_KLOBUCHAR_REQUEST |
					NRF_MODEM_GNSS_AGPS_SYS_TIME_AND_SV_TOW_REQUEST |
					NRF_MODEM_GNSS_AGPS_POSITION_REQUEST |
					NRF_MODEM_GNSS_AGPS_INTEGRITY_REQUEST;
	} else {
		cloud_agps_request.request = *incoming_request;
	}

	cloud_agps_request.mcc = modem_info.network.mcc.value;
	cloud_agps_request.mnc = modem_info.network.mnc.value;
	cloud_agps_request.cell = modem_info.network.cellid_dec;
	cloud_agps_request.area = modem_info.network.area_code.value;
	cloud_agps_request.queued = true;
#if defined(CONFIG_GNSS_MODULE_AGPS_FILTERED)
	cloud_agps_request.filtered = CONFIG_GNSS_MODULE_AGPS_FILTERED;
#endif
#if defined(CONFIG_GNSS_MODULE_ELEVATION_MASK)
	cloud_agps_request.mask_angle = CONFIG_GNSS_MODULE_ELEVATION_MASK;
#endif

	err = cloud_codec_encode_agps_request(&codec, &cloud_agps_request);
	switch (err) {
	case 0:
		LOG_DBG("A-GPS request encoded successfully");
		data_send(DATA_EVT_AGPS_REQUEST_DATA_SEND, &codec);
		break;
	case -ENOTSUP:
		LOG_WRN("Encoding of A-GPS requests are not supported by the configured codec");
		break;
	case -ENODATA:
		LOG_DBG("No A-GPS request data to encode, error: %d", err);
		break;
	default:
		LOG_ERR("Error encoding A-GPS request: %d", err);
		SEND_ERROR(data, DATA_EVT_ERROR, err);
		break;
	}

	return err;
}
#endif /* CONFIG_NRF_CLOUD_AGPS */

static void config_get(void)
{
	SEND_EVENT(data, DATA_EVT_CONFIG_GET);
}

static void data_ui_send(void)
{
	int err;
	struct cloud_codec_data codec = {0};

	if (!date_time_is_valid()) {
		/* Date time library does not have valid time to
		 * timestamp cloud data. Abort cloud publicaton. Data will
		 * be cached in it respective ringbuffer.
		 */
		return;
	}

	err = cloud_codec_encode_ui_data(&codec, &ui_buf);
	if (err == -ENODATA) {
		LOG_DBG("No new UI data to encode, error: %d", err);
		return;
	} else if (err == -ENOTSUP) {
		LOG_WRN("Encoding of UI data is not supported, error: %d", err);
		return;
	} else if (err) {
		LOG_ERR("Encoding button press, error: %d", err);
		SEND_ERROR(data, DATA_EVT_ERROR, err);
		return;
	}

	data_send(DATA_EVT_UI_DATA_SEND, &codec);
}

static void requested_data_clear(void)
{
	recv_req_data_count = 0;
	req_data_count = 0;
}

static void data_send_work_fn(struct k_work *work)
{
	SEND_EVENT(data, DATA_EVT_DATA_READY);

	requested_data_clear();
	k_work_cancel_delayable(&data_send_work);
}

static void requested_data_status_set(enum app_module_data_type data_type)
{
	if (!k_work_delayable_is_pending(&data_send_work)) {
		/* If the data_send_work is not pending it means that the module has already
		 * triggered an data encode/send.
		 */
		LOG_DBG("Data already encoded and sent, abort.");
		return;
	}

	for (size_t i = 0; i < recv_req_data_count; i++) {
		if (req_type_list[i] == data_type) {
			req_data_count++;
			break;
		}
	}

	if (req_data_count == recv_req_data_count) {
		data_send_work_fn(NULL);
	}
}

static void requested_data_list_set(enum app_module_data_type *data_list,
				    size_t count)
{
	if ((count == 0) || (count > APP_DATA_COUNT)) {
		LOG_ERR("Invalid data type list length");
		return;
	}

	requested_data_clear();

	for (size_t i = 0; i < count; i++) {
		req_type_list[i] = data_list[i];
	}

	recv_req_data_count = count;
}

static void new_config_handle(struct cloud_data_cfg *new_config)
{
	memcpy(&current_cfg, new_config, sizeof(struct cloud_data_cfg));

	/* If there has been a change in the currently applied device configuration we want to store
	 * the configuration to flash and distribute it to other modules.
	 */
	int err = save_config(&current_cfg, sizeof(current_cfg));

	if (err) {
		LOG_WRN("Configuration not stored, error: %d", err);
	}

	config_distribute(DATA_EVT_CONFIG_READY);
}

/**
 * @brief Function that requests A-GPS and P-GPS data upon receiving a request from the GNSS module.
 *	  If both A-GPS and P-GPS is enabled. A-GPS will take precedence.
 *
 * @param[in] incoming_request Pointer to a structure containing A-GPS data types that has been
 *			       requested by the modem. If incoming_request is NULL, all A-GPS data
 *			       types are requested.
 */
static void agps_request_handle(struct nrf_modem_gnss_agps_data_frame *incoming_request)
{
	int err;

#if defined(CONFIG_NRF_CLOUD_AGPS)
	struct nrf_modem_gnss_agps_data_frame request;

	if (incoming_request != NULL) {
		request.sv_mask_ephe = IS_ENABLED(CONFIG_NRF_CLOUD_PGPS) ?
				       0u : incoming_request->sv_mask_ephe;
		request.sv_mask_alm = IS_ENABLED(CONFIG_NRF_CLOUD_PGPS) ?
				       0u : incoming_request->sv_mask_alm;
		request.data_flags = incoming_request->data_flags;
	}

	/* If the nRF Cloud MQTT transport library is not enabled, we will have to create an
	 * A-GPS request and send out an event containing the request for the cloud module to pick
	 * up and send to the cloud that is currently used.
	 */
	err = (incoming_request == NULL) ? agps_request_encode(NULL) :
					   agps_request_encode(&request);
	if (err) {
		LOG_WRN("Failed to request A-GPS data, error: %d", err);
	} else {
		LOG_DBG("A-GPS request sent");
		return;
	}
#endif

#if defined(CONFIG_NRF_CLOUD_PGPS)
	/* A-GPS data is not expected to be received. Proceed to schedule a callback when
	 * P-GPS data for current time is available.
	 */
	err = nrf_cloud_pgps_notify_prediction();
	if (err) {
		LOG_ERR("Requesting notification of prediction availability, error: %d", err);
	}
#endif

	(void)err;
}

/* Message handler for STATE_CLOUD_DISCONNECTED. */
static void on_cloud_state_disconnected(struct data_msg_data *msg)
{
	if (IS_EVENT(msg, cloud, CLOUD_EVT_CONNECTED)) {
		state_set(STATE_CLOUD_CONNECTED);
		return;
	}
}

/* Message handler for STATE_CLOUD_CONNECTED. */
static void on_cloud_state_connected(struct data_msg_data *msg)
{
	if (IS_EVENT(msg, data, DATA_EVT_DATA_READY)) {
		data_encode();
		return;
	}

	if (IS_EVENT(msg, app, APP_EVT_CONFIG_GET)) {
		config_get();
		return;
	}

	if (IS_EVENT(msg, data, DATA_EVT_UI_DATA_READY)) {
		data_ui_send();
		return;
	}

	if (IS_EVENT(msg, cloud, CLOUD_EVT_DISCONNECTED)) {
		state_set(STATE_CLOUD_DISCONNECTED);
		return;
	}

	if (IS_EVENT(msg, app, APP_EVT_AGPS_NEEDED)) {
		agps_request_handle(NULL);
		return;
	}
}

/* Message handler for all states. */
static void on_all_states(struct data_msg_data *msg)
{
	/* Distribute new configuration received from cloud. */
	if (IS_EVENT(msg, cloud, CLOUD_EVT_CONFIG_RECEIVED)) {
		new_config_handle(&msg->module.cloud.data.config);
		return;
	}

	if (IS_EVENT(msg, gnss, GNSS_EVT_AGPS_NEEDED)) {
		agps_request_handle(&msg->module.gnss.data.agps_request);
		return;
	}

	if (IS_EVENT(msg, app, APP_EVT_START)) {
		config_print_all();
		config_distribute(DATA_EVT_CONFIG_INIT);
	}

	if (IS_EVENT(msg, util, UTIL_EVT_SHUTDOWN_REQUEST)) {
		/* The module doesn't have anything to shut down and can
		 * report back immediately.
		 */
		SEND_SHUTDOWN_ACK(data, DATA_EVT_SHUTDOWN_READY, self.id);
		state_set(STATE_SHUTDOWN);
	}

	if (IS_EVENT(msg, app, APP_EVT_DATA_GET)) {
		/* Store which data is requested by the app, later to be used
		 * to confirm data is reported to the data manger.
		 */
		requested_data_list_set(msg->module.app.data_list,
					msg->module.app.count);

		/* Start countdown until data must have been received by the
		 * Data module in order to be sent to cloud
		 */
		k_work_reschedule(&data_send_work,
				      K_SECONDS(msg->module.app.timeout));

		return;
	}

	if (IS_EVENT(msg, ui, UI_EVT_BUTTON_DATA_READY)) {
		ui_buf.btn = msg->module.ui.data.ui.button_number;
		ui_buf.btn_ts = msg->module.ui.data.ui.timestamp;
		ui_buf.queued = true;

		SEND_EVENT(data, DATA_EVT_UI_DATA_READY);
		return;
	}

	if (IS_EVENT(msg, modem, MODEM_EVT_MODEM_STATIC_DATA_NOT_READY)) {
		requested_data_status_set(APP_DATA_MODEM_STATIC);
	}

	if (IS_EVENT(msg, modem, MODEM_EVT_MODEM_STATIC_DATA_READY)) {
		modem_stat.ts = msg->module.modem.data.modem_static.timestamp;
		modem_stat.queued = true;

		BUILD_ASSERT(sizeof(modem_stat.appv) >=
			     sizeof(msg->module.modem.data.modem_static.app_version));

		BUILD_ASSERT(sizeof(modem_stat.brdv) >=
			     sizeof(msg->module.modem.data.modem_static.board_version));

		BUILD_ASSERT(sizeof(modem_stat.fw) >=
			     sizeof(msg->module.modem.data.modem_static.modem_fw));

		BUILD_ASSERT(sizeof(modem_stat.iccid) >=
			     sizeof(msg->module.modem.data.modem_static.iccid));

		BUILD_ASSERT(sizeof(modem_stat.imei) >=
			     sizeof(msg->module.modem.data.modem_static.imei));

		strcpy(modem_stat.appv, msg->module.modem.data.modem_static.app_version);
		strcpy(modem_stat.brdv, msg->module.modem.data.modem_static.board_version);
		strcpy(modem_stat.fw, msg->module.modem.data.modem_static.modem_fw);
		strcpy(modem_stat.iccid, msg->module.modem.data.modem_static.iccid);
		strcpy(modem_stat.imei, msg->module.modem.data.modem_static.imei);

		requested_data_status_set(APP_DATA_MODEM_STATIC);
	}

	if (IS_EVENT(msg, modem, MODEM_EVT_MODEM_DYNAMIC_DATA_NOT_READY)) {
		requested_data_status_set(APP_DATA_MODEM_DYNAMIC);
	}

	if (IS_EVENT(msg, modem, MODEM_EVT_MODEM_DYNAMIC_DATA_READY)) {
		modem_dyn_buf.area = msg->module.modem.data.modem_dynamic.area_code;
		modem_dyn_buf.nw_mode = msg->module.modem.data.modem_dynamic.nw_mode;
		modem_dyn_buf.band = msg->module.modem.data.modem_dynamic.band;
		modem_dyn_buf.cell = msg->module.modem.data.modem_dynamic.cell_id;
		modem_dyn_buf.rsrp = msg->module.modem.data.modem_dynamic.rsrp;
		modem_dyn_buf.mcc = msg->module.modem.data.modem_dynamic.mcc;
		modem_dyn_buf.mnc = msg->module.modem.data.modem_dynamic.mnc;
		modem_dyn_buf.ts = msg->module.modem.data.modem_dynamic.timestamp;
		modem_dyn_buf.queued = true;

		BUILD_ASSERT(sizeof(modem_dyn_buf.ip) >=
			     sizeof(msg->module.modem.data.modem_dynamic.ip_address));

		BUILD_ASSERT(sizeof(modem_dyn_buf.apn) >=
			     sizeof(msg->module.modem.data.modem_dynamic.apn));

		BUILD_ASSERT(sizeof(modem_dyn_buf.apn) >=
			     sizeof(msg->module.modem.data.modem_dynamic.apn));

		strcpy(modem_dyn_buf.ip, msg->module.modem.data.modem_dynamic.ip_address);
		strcpy(modem_dyn_buf.apn, msg->module.modem.data.modem_dynamic.apn);
		strcpy(modem_dyn_buf.mccmnc, msg->module.modem.data.modem_dynamic.mccmnc);

		requested_data_status_set(APP_DATA_MODEM_DYNAMIC);
	}

	if (IS_EVENT(msg, modem, MODEM_EVT_BATTERY_DATA_NOT_READY)) {
		requested_data_status_set(APP_DATA_BATTERY);
	}

	if (IS_EVENT(msg, modem, MODEM_EVT_BATTERY_DATA_READY)) {
		bat_buf.bat = msg->module.modem.data.bat.battery_voltage;
		bat_buf.bat_ts = msg->module.modem.data.bat.timestamp;
		bat_buf.queued = true;

		requested_data_status_set(APP_DATA_BATTERY);
	}

	if (IS_EVENT(msg, sensor, SENSOR_EVT_ENVIRONMENTAL_DATA_READY)) {
		sensors_buf.temperature = msg->module.sensor.data.sensors.temperature;
		sensors_buf.humidity = msg->module.sensor.data.sensors.humidity;
		sensors_buf.pressure = msg->module.sensor.data.sensors.pressure;
		sensors_buf.bsec_air_quality = msg->module.sensor.data.sensors.bsec_air_quality;
		sensors_buf.env_ts = msg->module.sensor.data.sensors.timestamp;
		sensors_buf.queued = true;

		requested_data_status_set(APP_DATA_ENVIRONMENTAL);
	}

	if (IS_EVENT(msg, sensor, SENSOR_EVT_ENVIRONMENTAL_NOT_SUPPORTED)) {
		requested_data_status_set(APP_DATA_ENVIRONMENTAL);
	}

	if (IS_EVENT(msg, gnss, GNSS_EVT_DATA_READY)) {
		gnss_buf.pvt.acc = msg->module.gnss.data.gnss.pvt.accuracy;
		gnss_buf.pvt.alt = msg->module.gnss.data.gnss.pvt.altitude;
		gnss_buf.pvt.hdg = msg->module.gnss.data.gnss.pvt.heading;
		gnss_buf.pvt.lat = msg->module.gnss.data.gnss.pvt.latitude;
		gnss_buf.pvt.longi = msg->module.gnss.data.gnss.pvt.longitude;
		gnss_buf.pvt.spd = msg->module.gnss.data.gnss.pvt.speed;
		gnss_buf.queued = true;
		requested_data_status_set(APP_DATA_GNSS);
	}

	if (IS_EVENT(msg, modem, MODEM_EVT_NEIGHBOR_CELLS_DATA_READY)) {
		BUILD_ASSERT(sizeof(neighbor_cells.cell_data) ==
			     sizeof(msg->module.modem.data.neighbor_cells.cell_data));

		BUILD_ASSERT(sizeof(neighbor_cells.neighbor_cells) ==
			     sizeof(msg->module.modem.data.neighbor_cells.neighbor_cells));

		memcpy(&neighbor_cells.cell_data, &msg->module.modem.data.neighbor_cells.cell_data,
		       sizeof(neighbor_cells.cell_data));

		memcpy(&neighbor_cells.neighbor_cells,
		       &msg->module.modem.data.neighbor_cells.neighbor_cells,
		       sizeof(neighbor_cells.neighbor_cells));

		neighbor_cells.ts = msg->module.modem.data.neighbor_cells.timestamp;
		neighbor_cells.queued = true;

		requested_data_status_set(APP_DATA_NEIGHBOR_CELLS);
	}

	if (IS_EVENT(msg, modem, MODEM_EVT_NEIGHBOR_CELLS_DATA_NOT_READY)) {
		requested_data_status_set(APP_DATA_NEIGHBOR_CELLS);
	}

	if (IS_EVENT(msg, gnss, GNSS_EVT_TIMEOUT)) {
		requested_data_status_set(APP_DATA_GNSS);
	}
}

static void module_thread_fn(void)
{
	int err;
	struct data_msg_data msg = { 0 };

	self.thread_id = k_current_get();

	err = module_start(&self);
	if (err) {
		LOG_ERR("Failed starting module, error: %d", err);
		SEND_ERROR(data, DATA_EVT_ERROR, err);
	}

	state_set(STATE_CLOUD_DISCONNECTED);

	k_work_init_delayable(&data_send_work, data_send_work_fn);

	err = setup();
	if (err) {
		LOG_ERR("setup, error: %d", err);
		SEND_ERROR(data, DATA_EVT_ERROR, err);
	}

	while (true) {
		module_get_next_msg(&self, &msg);

		switch (state) {
		case STATE_CLOUD_DISCONNECTED:
			on_cloud_state_disconnected(&msg);
			break;
		case STATE_CLOUD_CONNECTED:
			on_cloud_state_connected(&msg);
			break;
		case STATE_SHUTDOWN:
			/* The shutdown state has no transition. */
			break;
		default:
			LOG_WRN("Unknown sub state.");
			break;
		}

		on_all_states(&msg);
	}
}

K_THREAD_DEFINE(data_module_thread, CONFIG_DATA_THREAD_STACK_SIZE,
		module_thread_fn, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, app_module_event);
APP_EVENT_SUBSCRIBE(MODULE, util_module_event);
APP_EVENT_SUBSCRIBE(MODULE, data_module_event);
APP_EVENT_SUBSCRIBE_EARLY(MODULE, modem_module_event);
APP_EVENT_SUBSCRIBE_EARLY(MODULE, cloud_module_event);
APP_EVENT_SUBSCRIBE_EARLY(MODULE, gnss_module_event);
APP_EVENT_SUBSCRIBE_EARLY(MODULE, ui_module_event);
APP_EVENT_SUBSCRIBE_EARLY(MODULE, sensor_module_event);
