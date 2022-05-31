/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <stdio.h>
#include <zephyr/device.h>
#include <app_event_manager.h>
#include "modules_common.h"

#define MODULE display

#include "events/gnss_module_event.h"
#include "events/modem_module_event.h"
#include "events/led_state_event.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(MODULE, CONFIG_DISPLAY_LOG_LEVEL);

struct display_msg_data {
	union {
		struct modem_module_event modem;
		struct gnss_module_event gnss;
	} module;
};

/* Forward declarations. */
static void message_handler(struct display_msg_data *msg);

/* Handlers */
static bool app_event_handler(const struct app_event_header *aeh)
{
	if (is_modem_module_event(aeh)) {
		struct modem_module_event *event = cast_modem_module_event(aeh);
		struct display_msg_data ui_msg = {
			.module.modem = *event
		};

		message_handler(&ui_msg);
	}

	if (is_gnss_module_event(aeh)) {
		struct gnss_module_event *event = cast_gnss_module_event(aeh);
		struct display_msg_data ui_msg = {
			.module.gnss = *event
		};

		message_handler(&ui_msg);
	}

	return false;
}

static int setup(const struct device *dev)
{
	ARG_UNUSED(dev);

	/* Init */

	return 0;
}

static void message_handler(struct display_msg_data *msg)
{
	if (IS_EVENT(msg, modem, MODEM_EVT_LTE_CONNECTED)) {
		LOG_WRN("MODEM_EVT_LTE_CONNECTED");
	}

	if (IS_EVENT(msg, modem, MODEM_EVT_LTE_CELL_UPDATE)) {
		LOG_WRN("MODEM_EVT_LTE_CELL_UPDATE");

		LOG_WRN("CELL ID: %d", msg->module.modem.data.cell.cell_id);
		LOG_WRN("Tracking Are Code: %d", msg->module.modem.data.cell.tac);
	}

	if (IS_EVENT(msg, modem, MODEM_EVT_BATTERY_DATA_READY)) {
		LOG_WRN("MODEM_EVT_BATTERY_DATA_READY");

		LOG_WRN("Voltage level %d at timestamp: %lld",
			msg->module.modem.data.bat.battery_voltage,
			msg->module.modem.data.bat.timestamp);
	}

	if (IS_EVENT(msg, modem, MODEM_EVT_MODEM_DYNAMIC_DATA_READY)) {
		LOG_WRN("MODEM_EVT_MODEM_DYNAMIC_DATA_READY");

		LOG_WRN("RSRP level %d at timestamp: %lld",
			msg->module.modem.data.modem_dynamic.rsrp,
			msg->module.modem.data.modem_dynamic.timestamp);
	}

	if (IS_EVENT(msg, gnss, GNSS_EVT_ACTIVE)) {
		LOG_WRN("GNSS_EVT_ACTIVE");
	}

	if (IS_EVENT(msg, gnss, GNSS_EVT_INACTIVE)) {
		LOG_WRN("GNSS_EVT_INACTIVE");
	}
}

APP_EVENT_LISTENER(MODULE, app_event_handler);
APP_EVENT_SUBSCRIBE_EARLY(MODULE, gnss_module_event);
APP_EVENT_SUBSCRIBE_EARLY(MODULE, modem_module_event);

SYS_INIT(setup, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
