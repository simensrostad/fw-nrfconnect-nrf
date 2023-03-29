/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(sampler, CONFIG_MQTT_SAMPLE_SAMPLER_LOG_LEVEL);

/* Register subscriber */
ZBUS_SUBSCRIBER_DEFINE(sampler, CONFIG_MQTT_SAMPLE_SAMPLER_MESSAGE_QUEUE_SIZE);

static void sample(void)
{
	int err;
	struct payload payload = {
		.raw.id = 24,
		.raw.type = "Dog",
		.raw.name = "Charlie",
		.raw.uptime = k_uptime_get_32()
	};

	err = zbus_chan_pub(&PAYLOAD_CHAN, &payload, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error:%d", err);
		SEND_FATAL_ERROR();
	}
}

static void sampler_task(void)
{
	const struct zbus_channel *chan;

	while (!zbus_sub_wait(&sampler, &chan, K_FOREVER)) {
		if (&TRIGGER_CHAN == chan) {
			sample();
		}
	}
}

K_THREAD_DEFINE(sampler_task_id,
		CONFIG_MQTT_SAMPLE_SAMPLER_THREAD_STACK_SIZE,
		sampler_task, NULL, NULL, NULL, 3, 0, 0);
