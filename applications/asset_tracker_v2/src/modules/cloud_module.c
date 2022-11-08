/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/net/socket.h>
#include <stdio.h>
#include <zephyr/dfu/mcuboot.h>
#include <math.h>
#include <app_event_manager.h>

#include "lwm2m_integration.h"
#include "codec/lwm2m_codec.h"

#define MODULE cloud_module

#include "modules_common.h"
#include "events/cloud_module_event.h"
#include "events/app_module_event.h"
#include "events/data_module_event.h"
#include "events/util_module_event.h"
#include "events/modem_module_event.h"
#include "events/gnss_module_event.h"
#include "events/debug_module_event.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_CLOUD_MODULE_LOG_LEVEL);

BUILD_ASSERT(CONFIG_CLOUD_CONNECT_RETRIES < 14,
	     "Cloud connect retries too large");
struct cloud_msg_data {
	union {
		struct app_module_event app;
		struct data_module_event data;
		struct modem_module_event modem;
		struct cloud_module_event cloud;
		struct util_module_event util;
		struct gnss_module_event gnss;
		struct debug_module_event debug;
	} module;
};

/* Cloud module super states. */
static enum state_type {
	STATE_LTE_INIT,
	STATE_LTE_DISCONNECTED,
	STATE_LTE_CONNECTED,
	STATE_SHUTDOWN
} state;

/* Cloud module sub states. */
static enum sub_state_type {
	SUB_STATE_CLOUD_DISCONNECTED,
	SUB_STATE_CLOUD_CONNECTED
} sub_state;

static struct k_work_delayable connect_check_work;

struct cloud_backoff_delay_lookup {
	int delay;
};

/* Lookup table for backoff reconnection to cloud. Binary scaling. */
static struct cloud_backoff_delay_lookup backoff_delay[] = {
	{ 64 }, { 128 }, { 256 }, { 512 },
	{ 2048 }, { 4096 }, { 8192 }, { 16384 }, { 32768 },
	{ 65536 }, { 131072 }, { 262144 }, { 524288 }, { 1048576 }
};

/* Variable that keeps track of how many times a reconnection to cloud
 * has been tried without success.
 */
static int connect_retries;

/* Local copy of the device configuration. */
static struct cloud_data_cfg copy_cfg;
const k_tid_t cloud_module_thread;

#if defined(CONFIG_NRF_CLOUD_PGPS)
/* Local copy of the last A-GPS request from the modem, used to inject correct P-GPS data. */
static struct nrf_modem_gnss_agps_data_frame agps_request = {
	.sv_mask_ephe = 0xFFFFFFFF,
	.sv_mask_alm = 0xFFFFFFFF,
	.data_flags = NRF_MODEM_GNSS_AGPS_GPS_UTC_REQUEST |
		      NRF_MODEM_GNSS_AGPS_KLOBUCHAR_REQUEST |
		      NRF_MODEM_GNSS_AGPS_SYS_TIME_AND_SV_TOW_REQUEST |
		      NRF_MODEM_GNSS_AGPS_POSITION_REQUEST |
		      NRF_MODEM_GNSS_AGPS_INTEGRITY_REQUEST
};
#endif

/* Cloud module message queue. */
#define CLOUD_QUEUE_ENTRY_COUNT		20
#define CLOUD_QUEUE_BYTE_ALIGNMENT	4

K_MSGQ_DEFINE(msgq_cloud, sizeof(struct cloud_msg_data),
	      CLOUD_QUEUE_ENTRY_COUNT, CLOUD_QUEUE_BYTE_ALIGNMENT);

static struct module_data self = {
	.name = "cloud",
	.msg_q = &msgq_cloud,
	.supports_shutdown = true
};

/* Forward declarations. */
static void connect_check_work_fn(struct k_work *work);

/* Convenience functions used in internal state handling. */
static char *state2str(enum state_type state)
{
	switch (state) {
	case STATE_LTE_INIT:
		return "STATE_LTE_INIT";
	case STATE_LTE_DISCONNECTED:
		return "STATE_LTE_DISCONNECTED";
	case STATE_LTE_CONNECTED:
		return "STATE_LTE_CONNECTED";
	case STATE_SHUTDOWN:
		return "STATE_SHUTDOWN";
	default:
		return "Unknown";
	}
}

static char *sub_state2str(enum sub_state_type new_state)
{
	switch (new_state) {
	case SUB_STATE_CLOUD_DISCONNECTED:
		return "SUB_STATE_CLOUD_DISCONNECTED";
	case SUB_STATE_CLOUD_CONNECTED:
		return "SUB_STATE_CLOUD_CONNECTED";
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

static void sub_state_set(enum sub_state_type new_state)
{
	if (new_state == sub_state) {
		LOG_DBG("Sub state: %s", sub_state2str(sub_state));
		return;
	}

	LOG_DBG("Sub state transition %s --> %s",
		sub_state2str(sub_state),
		sub_state2str(new_state));

	sub_state = new_state;
}

/* Handlers */
static bool app_event_handler(const struct app_event_header *aeh)
{
	struct cloud_msg_data msg = {0};
	bool enqueue_msg = false, consume = false;

	if (is_app_module_event(aeh)) {
		struct app_module_event *evt = cast_app_module_event(aeh);

		msg.module.app = *evt;
		enqueue_msg = true;
	}

	if (is_data_module_event(aeh)) {
		struct data_module_event *evt = cast_data_module_event(aeh);

		msg.module.data = *evt;
		enqueue_msg = true;
	}

	if (is_modem_module_event(aeh)) {
		struct modem_module_event *evt = cast_modem_module_event(aeh);

		msg.module.modem = *evt;
		enqueue_msg = true;
	}

	if (is_cloud_module_event(aeh)) {
		struct cloud_module_event *evt = cast_cloud_module_event(aeh);

		msg.module.cloud = *evt;
		enqueue_msg = true;
	}

	if (is_util_module_event(aeh)) {
		struct util_module_event *evt = cast_util_module_event(aeh);

		msg.module.util = *evt;
		enqueue_msg = true;
	}

	if (is_gnss_module_event(aeh)) {
		struct gnss_module_event *evt = cast_gnss_module_event(aeh);

		msg.module.gnss = *evt;
		enqueue_msg = true;
	}

	if (is_debug_module_event(aeh)) {
		struct debug_module_event *evt = cast_debug_module_event(aeh);

		msg.module.debug = *evt;
		enqueue_msg = true;
	}

	if (enqueue_msg) {
		int err = module_enqueue_msg(&self, &msg);

		if (err) {
			LOG_ERR("Message could not be enqueued");
			SEND_ERROR(cloud, CLOUD_EVT_ERROR, err);
		}
	}

	return consume;
}

static void cloud_wrap_event_handler(const struct cloud_wrap_event *const evt)
{
	switch (evt->type) {
	case CLOUD_WRAP_EVT_CONNECTING: {
		LOG_DBG("CLOUD_WRAP_EVT_CONNECTING");
		SEND_EVENT(cloud, CLOUD_EVT_CONNECTING);
		break;
	}
	case CLOUD_WRAP_EVT_CONNECTED: {
		LOG_DBG("CLOUD_WRAP_EVT_CONNECTED");
		SEND_EVENT(cloud, CLOUD_EVT_CONNECTED);
		break;
	}
	case CLOUD_WRAP_EVT_DISCONNECTED: {
		LOG_DBG("CLOUD_WRAP_EVT_DISCONNECTED");
		SEND_EVENT(cloud, CLOUD_EVT_DISCONNECTED);
		break;
	}
	case CLOUD_WRAP_EVT_REBOOT_REQUEST: {
		SEND_EVENT(cloud, CLOUD_EVT_REBOOT_REQUEST);
	};
		break;
	case CLOUD_WRAP_EVT_LTE_DISCONNECT_REQUEST: {
		SEND_EVENT(cloud, CLOUD_EVT_LTE_DISCONNECT);
	};
		break;
	case CLOUD_WRAP_EVT_LTE_CONNECT_REQUEST: {
		SEND_EVENT(cloud, CLOUD_EVT_LTE_CONNECT);
	};
		break;
	case CLOUD_WRAP_EVT_FOTA_DONE: {
		LOG_DBG("CLOUD_WRAP_EVT_FOTA_DONE");
		SEND_EVENT(cloud, CLOUD_EVT_FOTA_DONE);
		break;
	}
	case CLOUD_WRAP_EVT_FOTA_START: {
		LOG_DBG("CLOUD_WRAP_EVT_FOTA_START");
		SEND_EVENT(cloud, CLOUD_EVT_FOTA_START);
		break;
	}
	case CLOUD_WRAP_EVT_FOTA_ERASE_PENDING:
		LOG_DBG("CLOUD_WRAP_EVT_FOTA_ERASE_PENDING");
		break;
	case CLOUD_WRAP_EVT_FOTA_ERASE_DONE:
		LOG_DBG("CLOUD_WRAP_EVT_FOTA_ERASE_DONE");
		break;
	case CLOUD_WRAP_EVT_FOTA_ERROR: {
		LOG_DBG("CLOUD_WRAP_EVT_FOTA_ERROR");
		SEND_EVENT(cloud, CLOUD_EVT_FOTA_ERROR);
		break;
	}
	case CLOUD_WRAP_EVT_ERROR: {
		LOG_DBG("CLOUD_WRAP_EVT_ERROR");
		SEND_ERROR(cloud, CLOUD_EVT_ERROR, evt->err);
		break;
	}
	default:
		break;

	}
}

static void connect_cloud(void)
{
	int err;
	int backoff_sec = backoff_delay[connect_retries].delay;

	LOG_DBG("Connecting to cloud");

	if (connect_retries > CONFIG_CLOUD_CONNECT_RETRIES) {
		LOG_ERR("Too many failed cloud connection attempts");
		SEND_ERROR(cloud, CLOUD_EVT_ERROR, -ENETUNREACH);
		return;
	}

	/* The cloud will return error if cloud_wrap_connect() is called while
	 * the socket is polled on in the internal cloud thread or the
	 * cloud backend is the wrong state. We cannot treat this as an error as
	 * it is rather common that cloud_connect can be called under these
	 * conditions.
	 */
	err = cloud_wrap_connect();
	if (err == -EINPROGRESS) {
		LOG_DBG("Cloud connection in progress");
	} else if (err) {
		LOG_DBG("cloud_connect failed, error: %d", err);
		SEND_ERROR(cloud, CLOUD_EVT_ERROR, -ENETUNREACH);
		return;
	}

	connect_retries++;

	LOG_DBG("Cloud connection establishment in progress");
	LOG_DBG("New connection attempt in %d seconds if not successful",
		backoff_sec);

	/* Start timer to check connection status after backoff */
	k_work_reschedule(&connect_check_work, K_SECONDS(backoff_sec));
}

static void disconnect_cloud(void)
{
	cloud_wrap_disconnect();

	connect_retries = 0;

	k_work_cancel_delayable(&connect_check_work);
}

/* If this work is executed, it means that the connection attempt was not
 * successful before the backoff timer expired. A timeout message is then
 * added to the message queue to signal the timeout.
 */
static void connect_check_work_fn(struct k_work *work)
{
	if ((state == STATE_LTE_CONNECTED && sub_state == SUB_STATE_CLOUD_CONNECTED) ||
	    (state == STATE_LTE_DISCONNECTED)) {
		return;
	}

	LOG_DBG("Cloud connection timeout occurred");

	SEND_EVENT(cloud, CLOUD_EVT_CONNECTION_TIMEOUT);
}

static int setup(void)
{
	int err;

	err = cloud_wrap_init(cloud_wrap_event_handler);
	if (err) {
		LOG_ERR("cloud_wrap_init, error: %d", err);
		return err;
	}

#if defined(CONFIG_MCUBOOT_IMG_MANAGER)
	/* After a successful initializaton, tell the bootloader that the
	 * current image is confirmed to be working.
	 */
	boot_write_img_confirmed();
#endif /* CONFIG_MCUBOOT_IMG_MANAGER */

	return 0;
}

/* Message handler for STATE_LTE_INIT. */
static void on_state_init(struct cloud_msg_data *msg)
{
	if (IS_EVENT(msg, modem, MODEM_EVT_INITIALIZED)) {
		state_set(STATE_LTE_DISCONNECTED);

		int err = setup();

		__ASSERT(err == 0, "setp() failed");
	}
}

/* Message handler for STATE_LTE_CONNECTED. */
static void on_state_lte_connected(struct cloud_msg_data *msg)
{
	if (IS_EVENT(msg, modem, MODEM_EVT_LTE_DISCONNECTED)) {
		sub_state_set(SUB_STATE_CLOUD_DISCONNECTED);
		state_set(STATE_LTE_DISCONNECTED);

		/* Explicitly disconnect cloud when you receive an LTE disconnected event.
		 * This is to clear up the cloud library state.
		 */
		disconnect_cloud();
	}

	if (IS_EVENT(msg, modem, MODEM_EVT_CARRIER_FOTA_PENDING)) {
		sub_state_set(SUB_STATE_CLOUD_DISCONNECTED);
		disconnect_cloud();
	}

	if (IS_EVENT(msg, modem, MODEM_EVT_CARRIER_FOTA_STOPPED)) {
		connect_cloud();
	}
}

/* Message handler for STATE_LTE_DISCONNECTED. */
static void on_state_lte_disconnected(struct cloud_msg_data *msg)
{
	if (IS_EVENT(msg, modem, MODEM_EVT_LTE_CONNECTED)) {
		state_set(STATE_LTE_CONNECTED);

		/* LTE is now connected, cloud connection can be attempted */
		connect_cloud();
	}
}

/* Message handler for SUB_STATE_CLOUD_CONNECTED. */
static void on_sub_state_cloud_connected(struct cloud_msg_data *msg)
{
	int err = 0;

	if (IS_EVENT(msg, cloud, CLOUD_EVT_DISCONNECTED)) {
		sub_state_set(SUB_STATE_CLOUD_DISCONNECTED);

		k_work_reschedule(&connect_check_work, K_SECONDS(1));
		return;
	}

	if (IS_EVENT(msg, debug, DEBUG_EVT_MEMFAULT_DATA_READY)) {
		/* Own LwM2M API for Memfault */
	}

	if (IS_EVENT(msg, data, DATA_EVT_AGPS_REQUEST_DATA_SEND)) {
		int err = cloud_wrap_agps_request_send(NULL,
							0,
							true,
							0);
		if (err) {
			LOG_ERR("cloud_wrap_agps_request_send, err: %d", err);
		}
	}

	if (IS_EVENT(msg, data, DATA_EVT_DATA_SEND)) {
		char *paths[CONFIG_CLOUD_CODEC_LWM2M_PATH_LIST_ENTRIES_MAX];

		__ASSERT(ARRAY_SIZE(paths) ==
				ARRAY_SIZE(msg->module.data.data.buffer.paths),
				"Path object list not the same size");

		for (int i = 0; i < ARRAY_SIZE(paths); i++) {
			paths[i] = msg->module.data.data.buffer.paths[i];
		}

		err = cloud_wrap_data_send(NULL,
						msg->module.data.data.buffer.valid_object_paths,
						true,
						0,
						paths);
		if (err) {
			LOG_ERR("cloud_wrap_data_send, err: %d", err);
		}

		return;
	}

	if (IS_EVENT(msg, data, DATA_EVT_UI_DATA_SEND)) {
		char *paths[CONFIG_CLOUD_CODEC_LWM2M_PATH_LIST_ENTRIES_MAX];

		__ASSERT(ARRAY_SIZE(paths) ==
				ARRAY_SIZE(msg->module.data.data.buffer.paths),
				"Path object list not the same size");

		for (int i = 0; i < ARRAY_SIZE(paths); i++) {
			paths[i] = msg->module.data.data.buffer.paths[i];
		}

		err = cloud_wrap_ui_send(NULL,
					 msg->module.data.data.buffer.valid_object_paths,
					 true,
					 0,
					 paths);
		if (err) {
			LOG_ERR("cloud_wrap_ui_send, err: %d", err);
		}
	}

	if (IS_EVENT(msg, data, DATA_EVT_NEIGHBOR_CELLS_DATA_SEND)) {
		err = cloud_wrap_neighbor_cells_send(NULL, 0, true, 0);
		if (err) {
			LOG_ERR("cloud_wrap_neighbor_cells_send, err: %d", err);
		}

		return;
	}
}

/* Message handler for SUB_STATE_CLOUD_DISCONNECTED. */
static void on_sub_state_cloud_disconnected(struct cloud_msg_data *msg)
{
	if (IS_EVENT(msg, cloud, CLOUD_EVT_CONNECTED)) {
		sub_state_set(SUB_STATE_CLOUD_CONNECTED);

		connect_retries = 0;
		k_work_cancel_delayable(&connect_check_work);
	}

	if (IS_EVENT(msg, cloud, CLOUD_EVT_CONNECTION_TIMEOUT)) {
		connect_cloud();
	}
}

/* Message handler for all states. */
static void on_all_states(struct cloud_msg_data *msg)
{
	if (IS_EVENT(msg, util, UTIL_EVT_SHUTDOWN_REQUEST)) {
		/* The module doesn't have anything to shut down and can
		 * report back immediately.
		 */
		SEND_SHUTDOWN_ACK(cloud, CLOUD_EVT_SHUTDOWN_READY, self.id);
		state_set(STATE_SHUTDOWN);
	}

	if (is_data_module_event(&msg->module.data.header)) {
		switch (msg->module.data.type) {
		case DATA_EVT_CONFIG_INIT:
			/* Fall through. */
		case DATA_EVT_CONFIG_READY:
			copy_cfg = msg->module.data.data.cfg;
			break;
		default:
			break;
		}
	}

#if defined(CONFIG_NRF_CLOUD_PGPS)
	if (IS_EVENT(msg, gnss, GNSS_EVT_AGPS_NEEDED)) {
		/* Keep a local copy of the incoming request. Used when injecting
		 * P-GPS data into the modem.
		 */
		memcpy(&agps_request, &msg->module.gnss.data.agps_request, sizeof(agps_request));
	}

	/* To properly initialize the nRF Cloud PGPS library date time must have been obtained. */
	if (IS_EVENT(msg, data, DATA_EVT_DATE_TIME_OBTAINED)) {
		struct nrf_cloud_pgps_init_param param = {
			.event_handler = pgps_handler,
			/* storage is defined by CONFIG_NRF_CLOUD_PGPS_STORAGE */
			.storage_base = 0u,
			.storage_size = 0u
		};

		int err = nrf_cloud_pgps_init(&param);

		if (err) {
			LOG_ERR("nrf_cloud_pgps_init: %d", err);
		}
	}
#endif
}

static void module_thread_fn(void)
{
	int err;
	struct cloud_msg_data msg = { 0 };

	self.thread_id = k_current_get();

	err = module_start(&self);
	if (err) {
		LOG_ERR("Failed starting module, error: %d", err);
		SEND_ERROR(cloud, CLOUD_EVT_ERROR, err);
	}

	state_set(STATE_LTE_INIT);
	sub_state_set(SUB_STATE_CLOUD_DISCONNECTED);

	k_work_init_delayable(&connect_check_work, connect_check_work_fn);

	while (true) {
		module_get_next_msg(&self, &msg);

		switch (state) {
		case STATE_LTE_INIT:
			on_state_init(&msg);
			break;
		case STATE_LTE_CONNECTED:
			switch (sub_state) {
			case SUB_STATE_CLOUD_CONNECTED:
				on_sub_state_cloud_connected(&msg);
				break;
			case SUB_STATE_CLOUD_DISCONNECTED:
				on_sub_state_cloud_disconnected(&msg);
				break;
			default:
				LOG_ERR("Unknown Cloud module sub state");
				break;
			}

			on_state_lte_connected(&msg);
			break;
		case STATE_LTE_DISCONNECTED:
			on_state_lte_disconnected(&msg);
			break;
		case STATE_SHUTDOWN:
			/* The shutdown state has no transition. */
			break;
		default:
			LOG_ERR("Unknown Cloud module state.");
			break;
		}

		on_all_states(&msg);
	}
}

K_THREAD_DEFINE(cloud_module_thread, CONFIG_CLOUD_THREAD_STACK_SIZE,
		module_thread_fn, NULL, NULL, NULL,
		K_LOWEST_APPLICATION_THREAD_PRIO, 0, 0);

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE(MODULE, data_module_event);
APP_EVENT_SUBSCRIBE(MODULE, app_module_event);
APP_EVENT_SUBSCRIBE(MODULE, modem_module_event);
APP_EVENT_SUBSCRIBE_FIRST(MODULE, cloud_module_event);
APP_EVENT_SUBSCRIBE(MODULE, gnss_module_event);
APP_EVENT_SUBSCRIBE(MODULE, debug_module_event);
APP_EVENT_SUBSCRIBE_EARLY(MODULE, util_module_event);
