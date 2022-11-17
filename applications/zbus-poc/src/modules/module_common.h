/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef _MODULE_COMMON_H_
#define _MODULE_COMMON_H_

#include "zephyr/kernel.h"

#ifdef __cplusplus
extern "C" {
#endif

struct module_common {
	int var;
};

ZBUS_CHAN_DEFINE(common_channel,					/* Name */
	 	 struct module_common,					/* Message type */
	 	 NULL,							/* Validator */
	 	 NULL,							/* User Data */
	 	 ZBUS_OBSERVERS(module_a),			/* observers */
	 	 ZBUS_MSG_INIT(.var = 0)				/* Initial value {0} */
);

#ifdef __cplusplus
}
#endif

#endif /* _MODULE_COMMON_H_ */
