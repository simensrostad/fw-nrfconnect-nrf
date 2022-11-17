/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "module_common.h"

#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

LOG_MODULE_REGISTER(module_a, CONFIG_APPLICATION_MODULE_LOG_LEVEL);

ZBUS_SUBSCRIBER_DEFINE(module_a, 4);

struct module_common message = {0};

void module_a_thread_fn(void)
{
	const struct zbus_channel *chan, int err;

	while (!zbus_sub_wait(&module_a, &chan, K_FOREVER)) {

		if (&common_channel == chan) {
			// Indirect message access
			zbus_chan_read(&common_channel, &message, K_NO_WAIT);
			LOG_DBG("Message variable: %d", message.var);
		}
	}
}
K_THREAD_DEFINE(MODULE, 512, module_a_thread_fn, NULL, NULL, NULL, 3, 0, 0);
