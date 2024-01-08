/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <unity.h>
#include <stdbool.h>
#include <stdlib.h>

#include "cmock_modules_common.h"
#include "cmock_app_event_manager.h"
#include "cmock_app_event_manager_priv.h"

/* Dummy structs to please linker. The APP_EVENT_SUBSCRIBE macros in debug_module.c
 * depend on these to exist. But since we are unit testing, we dont need
 * these subscriptions and hence these structs can remain uninitialized.
 */
struct event_type __event_type_app_module_event;
struct event_type __event_type_data_module_event;
struct event_type __event_type_modem_module_event;
struct event_type __event_type_sensor_module_event;
struct event_type __event_type_util_module_event;
struct event_type __event_type_cloud_module_event;
struct event_type __event_type_module_state_event;

/* It is required to be added to each test. That is because unity's
 * main may return nonzero, while zephyr's main currently must
 * return 0 in all cases (other values are reserved).
 */
extern int unity_main(void);

void test_should_expect_something(void)
{
	TEST_ASSERT_TRUE(true);
}

int main(void)
{
	(void)unity_main();
	return 0;
}
