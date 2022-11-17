/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* Module name is used by the Application Event Manager macros in this file */
#define MODULE main

#include "module_common.h"

LOG_MODULE_REGISTER(MODULE, CONFIG_APPLICATION_MODULE_LOG_LEVEL);

void main(void)
{
	k_sleep(K_SECONDS(5));

	while (true) {
		struct module_common message = {.var = 1};

		zbus_chan_pub(&common_channel, &acc1, K_SECONDS(5));
	}
}
