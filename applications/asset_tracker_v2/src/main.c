/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/smf.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <app_event_manager.h>
#include <zephyr/sys/reboot.h>

/* Module name is used by the Application Event Manager macros in this file */
#define MODULE main
#include <caf/events/module_state_event.h>

#include "modules_common.h"
#include "events/app_module_event.h"
#include "events/cloud_module_event.h"
#include "events/data_module_event.h"
#include "events/sensor_module_event.h"
#include "events/ui_module_event.h"
#include "events/util_module_event.h"
#include "events/modem_module_event.h"
#include "events/led_state_event.h"

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

LOG_MODULE_REGISTER(MODULE, CONFIG_APPLICATION_MODULE_LOG_LEVEL);

/* Message structure. Events from other modules are converted to messages
 * in the Application Event Manager handler, and then queued up in the message queue
 * for processing in the main thread.
 */
struct app_msg_data {
	union {
		struct cloud_module_event cloud;
		struct sensor_module_event sensor;
		struct data_module_event data;
		struct util_module_event util;
		struct modem_module_event modem;
		struct app_module_event app;
	} module;
};

/* Internal copy of the device configuration. */
static struct cloud_data_cfg app_cfg;

/* Application module message queue. */
#define QUEUE_ENTRY_COUNT	10
#define QUEUE_BYTE_ALIGNMENT	4

K_MSGQ_DEFINE(msgq_app, sizeof(struct app_msg_data), QUEUE_ENTRY_COUNT, QUEUE_BYTE_ALIGNMENT);

/* Timer callback used to signal when timeout has occurred both in active
 * and passive mode.
 */
static void data_sample_timer_handler(struct k_timer *timer);
static void data_get(void);

K_TIMER_DEFINE(data_sample_timer, data_sample_timer_handler, NULL);
K_TIMER_DEFINE(movement_timeout_timer, data_sample_timer_handler, NULL);

/* Module data structure to hold information of the application module, which
 * opens up for using convenience functions available for modules.
 */
static struct module_data self = {
	.name = "app",
	.msg_q = &msgq_app,
	.supports_shutdown = true,
};

/* Forward declaration of state table */
static const struct smf_state state[];

/* States */
enum demo_state { INIT, RUNNING, ACTIVE, PASSIVE, ACTIVITY, INACTIVITY};

/* User defined state object */
static struct s_object {
	/* This must be first */
	struct smf_ctx ctx;

	/* Application message data. */
	struct app_msg_data *msg;

} s_obj;

/* INIT State functions */
static void init_run(void *o)
{
	struct s_object *od = o;

	if (IS_EVENT(od->msg, data, DATA_EVT_CONFIG_INIT)) {
		/* Keep a copy of the new configuration. */
		app_cfg = od->msg->module.data.data.cfg;

		/* If app is in passive mode, go directly to inactivity, this implicitly changes
		 * the super-state to passive.
		 *
		 * Reset sensor module here to ensure that it is in an inactive state.
		 * or assume that the device is in an inactive state and sampling will be trigger
		 * upon the next activity event.
		 */
		smf_set_state(SMF_CTX(&s_obj), app_cfg.active_mode ? &state[ACTIVE] :
								     &state[INACTIVITY]);
	}
}

/* RUNNING State */
static void running_run(void *o)
{
	struct s_object *od = o;

	if ((IS_EVENT(od->msg, sensor, SENSOR_EVT_MOVEMENT_IMPACT_DETECTED)) ||
	    (IS_EVENT(od->msg, cloud, CLOUD_EVT_CONNECTED))) {
		data_get();
	}
}

/* ACTIVE State */
static void active_entry(void *o)
{
	k_timer_start(&data_sample_timer,
		      K_SECONDS(app_cfg.active_wait_timeout),
		      K_SECONDS(app_cfg.active_wait_timeout));
}

static void active_run(void *o)
{
	struct s_object *od = o;

	if (IS_EVENT(od->msg, data, DATA_EVT_CONFIG_READY)) {
		/* Keep a copy of the new configuration. */
		app_cfg = od->msg->module.data.data.cfg;

		if (app_cfg.active_mode) {
			return;
		}

		/* Reset sensor module here to ensure that it is in an inactive state.
		 * or assume that the device is in an inactive state and sampling will be trigger
		 * upon the next activity event.
		 */
		smf_set_state(SMF_CTX(&s_obj), &state[INACTIVITY]);
	}
}

/* PASSIVE State */
static void passive_entry(void *o)
{
	k_timer_start(&movement_timeout_timer,
		      K_SECONDS(app_cfg.movement_timeout),
		      K_SECONDS(app_cfg.movement_timeout));
}

static void passive_run(void *o)
{
	struct s_object *od = o;

	if (IS_EVENT(od->msg, data, DATA_EVT_CONFIG_READY)) {
		/* Keep a copy of the new configuration. */
		app_cfg = od->msg->module.data.data.cfg;

		if (app_cfg.active_mode == false) {
			return;
		}

		smf_set_state(SMF_CTX(&s_obj), &state[ACTIVE]);
	}
}

static void passive_exit(void *o)
{
	k_timer_stop(&movement_timeout_timer);
}

/* ACTIVITY State */
static void activity_entry(void *o)
{
	k_timer_start(&data_sample_timer,
		      K_NO_WAIT,
		      K_SECONDS(app_cfg.movement_resolution));
}

static void activity_run(void *o)
{
	struct s_object *od = o;

	if (IS_EVENT(od->msg, sensor, SENSOR_EVT_MOVEMENT_INACTIVITY_DETECTED)) {
		smf_set_state(SMF_CTX(&s_obj), &state[INACTIVITY]);
	}
}

static void activity_exit(void *o)
{
	/* When exiting activity we want to do an additional sample request. */
	data_get();
}

/* INACTIVITY State */
static void inactivity_entry(void *o)
{
	k_timer_stop(&data_sample_timer);
}

static void inactivity_run(void *o)
{
	struct s_object *od = o;

	if (IS_EVENT(od->msg, sensor, SENSOR_EVT_MOVEMENT_ACTIVITY_DETECTED)) {
		smf_set_state(SMF_CTX(&s_obj), &state[ACTIVITY]);
	}
}

/* Application Event Manager handler. Puts event data into messages and adds them to the
 * application message queue.
 */
static bool app_event_handler(const struct app_event_header *aeh)
{
	struct app_msg_data msg = {0};
	bool enqueue_msg = false;

	if (is_cloud_module_event(aeh)) {
		struct cloud_module_event *evt = cast_cloud_module_event(aeh);

		msg.module.cloud = *evt;
		enqueue_msg = true;
	}

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

	if (is_sensor_module_event(aeh)) {
		struct sensor_module_event *evt = cast_sensor_module_event(aeh);

		msg.module.sensor = *evt;
		enqueue_msg = true;
	}

	if (is_util_module_event(aeh)) {
		struct util_module_event *evt = cast_util_module_event(aeh);

		msg.module.util = *evt;
		enqueue_msg = true;
	}

	if (is_modem_module_event(aeh)) {
		struct modem_module_event *evt = cast_modem_module_event(aeh);

		msg.module.modem = *evt;
		enqueue_msg = true;
	}

	if (enqueue_msg) {
		int err = module_enqueue_msg(&self, &msg);

		if (err) {
			LOG_ERR("Message could not be enqueued");
			SEND_ERROR(app, APP_EVT_ERROR, err);
		}
	}

	return false;
}

static void data_sample_timer_handler(struct k_timer *timer)
{
	ARG_UNUSED(timer);
	data_get();
}

static void data_get(void)
{
	struct app_module_event *app_module_event = new_app_module_event();

	__ASSERT(app_module_event, "Not enough heap left to allocate event");

	size_t count = 0;

	/* Set a low sample timeout. If GNSS is requested, the sample timeout will be increased to
	 * accommodate the GNSS timeout.
	 */
	app_module_event->timeout = 120;

	/* Specify which data that is to be included in the transmission. */
	app_module_event->data_list[count++] = APP_DATA_MODEM_DYNAMIC;
	app_module_event->data_list[count++] = APP_DATA_BATTERY;
	app_module_event->data_list[count++] = APP_DATA_ENVIRONMENTAL;
	app_module_event->data_list[count++] = APP_DATA_MODEM_STATIC;
	app_module_event->data_list[count++] = APP_DATA_NEIGHBOR_CELLS;
	// app_module_event->data_list[count++] = APP_DATA_GNSS;

	/* Set list count to number of data types passed in app_module_event. */
	app_module_event->count = count;
	app_module_event->type = APP_EVT_DATA_GET;

	APP_EVENT_SUBMIT(app_module_event);
}

/* State table */
static const struct smf_state state[] = {
	[INIT] = SMF_CREATE_STATE(NULL, init_run, NULL, NULL),
	[RUNNING] = SMF_CREATE_STATE(NULL, running_run, NULL, NULL),
	[ACTIVE] = SMF_CREATE_STATE(active_entry, active_run, NULL, &state[RUNNING]),
	[PASSIVE] = SMF_CREATE_STATE(passive_entry, passive_run, passive_exit, &state[RUNNING]),
	[ACTIVITY] = SMF_CREATE_STATE(activity_entry, activity_run, activity_exit, &state[PASSIVE]),
	[INACTIVITY] = SMF_CREATE_STATE(inactivity_entry, inactivity_run, NULL, &state[PASSIVE]),
};

void main(void)
{
	int err;
	struct app_msg_data msg = { 0 };

	__ASSERT(app_event_manager_init() == 0, "Application Event Manager not be initialized");

	/* Start the rest of the system. */
	SEND_EVENT(app, APP_EVT_START);

	self.thread_id = k_current_get();

	err = module_start(&self);
	if (err) {
		LOG_ERR("Failed starting module, error: %d", err);
		SEND_ERROR(app, APP_EVT_ERROR, err);
	}

	smf_set_initial(SMF_CTX(&s_obj), &state[INIT]);

	while (true) {
		module_get_next_msg(&self, &msg);

		/* Set the new message. */
		s_obj.msg = &msg;

		err = smf_run_state(SMF_CTX(&s_obj));
		if (err) {
			__ASSERT(err == 0, "Error");
			break;
		}
	}
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE_EARLY(MODULE, cloud_module_event);
APP_EVENT_SUBSCRIBE(MODULE, app_module_event);
APP_EVENT_SUBSCRIBE(MODULE, data_module_event);
APP_EVENT_SUBSCRIBE(MODULE, util_module_event);
APP_EVENT_SUBSCRIBE_FINAL(MODULE, sensor_module_event);
APP_EVENT_SUBSCRIBE_FINAL(MODULE, modem_module_event);
