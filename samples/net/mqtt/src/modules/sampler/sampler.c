/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <modem/location.h>

#include "message_channel.h"

#define FORMAT_STRING "Hello MQTT! Current uptime is: %d"

/* Register log module */
LOG_MODULE_REGISTER(sampler, CONFIG_MQTT_SAMPLE_SAMPLER_LOG_LEVEL);

/* Register subscriber */
ZBUS_SUBSCRIBER_DEFINE(sampler, CONFIG_MQTT_SAMPLE_SAMPLER_MESSAGE_QUEUE_SIZE);

static void sample(void)
{
	int err;
	struct location_config config = { 0 };
	location_config_defaults_set(&config, 0, NULL);

	err = location_request(&config);
	if (err) {
		LOG_ERR("location_request, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
}

static void location_event_handler(const struct location_event_data *event_data)
{
	switch (event_data->id) {
	case LOCATION_EVT_LOCATION:
		LOG_INF("LOCATION_EVT_LOCATION");
		break;
	case LOCATION_EVT_RESULT_UNKNOWN:
		LOG_INF("LOCATION_EVT_RESULT_UNKNOWN");
		break;
	case LOCATION_EVT_TIMEOUT:
		LOG_INF("LOCATION_EVT_TIMEOUT");
		break;
	case LOCATION_EVT_ERROR:
		LOG_WRN("LOCATION_EVT_ERROR");
		break;
	case LOCATION_EVT_GNSS_ASSISTANCE_REQUEST:
		LOG_INF("LOCATION_EVT_GNSS_ASSISTANCE_REQUEST");
		break;
	case LOCATION_EVT_GNSS_PREDICTION_REQUEST:
		LOG_INF("LOCATION_EVT_GNSS_PREDICTION_REQUEST");
		break;
	case LOCATION_EVT_CLOUD_LOCATION_EXT_REQUEST: {
		LOG_INF("LOCATION_EVT_CLOUD_LOCATION_EXT_REQUEST");

		struct payload payload = { 0 };

		payload.network_location.cell_current = *(struct lte_lc_cells_info *)event_data->cloud_location_request.cell_data;
		memcpy(&payload.network_location.cell_neighbors, event_data->cloud_location_request.cell_data->neighbor_cells, sizeof(struct lte_lc_ncell) * event_data->cloud_location_request.cell_data->ncells_count);

		int err = zbus_chan_pub(&PAYLOAD_CHAN, &payload, K_SECONDS(1));

		if (err) {
			LOG_ERR("zbus_chan_pub, error:%d", err);
			SEND_FATAL_ERROR();
		}

		location_cloud_location_ext_result_set(LOCATION_EXT_RESULT_UNKNOWN, NULL);

		LOG_WRN("Number of neighbor cells: %d", event_data->cloud_location_request.cell_data->ncells_count);
	};

		break;

	default:
		LOG_INF("Getting location: Unknown event %d", event_data->id);
		break;
	}
}

static void sampler_task(void)
{
	int err;
	const struct zbus_channel *chan;

	err = location_init(location_event_handler);
	if (err) {
		LOG_ERR("location_init, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	while (!zbus_sub_wait(&sampler, &chan, K_FOREVER)) {
		if (&TRIGGER_CHAN == chan) {
			sample();
		}
	}
}

K_THREAD_DEFINE(sampler_task_id,
		CONFIG_MQTT_SAMPLE_SAMPLER_THREAD_STACK_SIZE,
		sampler_task, NULL, NULL, NULL, 3, 0, 0);
