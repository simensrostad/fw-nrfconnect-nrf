/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <zephyr/kernel.h>
#include <errno.h>
#include <unity.h>
#include <net/wifi_provision.h>

#include "cmock_wifi_credentials.h"

/* The unity_main is not declared in any header file. It is only defined in the generated test
 * runner because of ncs' unity configuration. It is therefore declared here to avoid a compiler
 * warning.
 */
extern int unity_main(void);

/* Verify wifi_provision_start() */

void test_init_should_do_something(void)
{
	TEST_ASSERT_EQUAL(-EINVAL, wifi_provision_start(NULL));
}

int main(void)
{
	(void)unity_main();

	return 0;
}
