/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */
#include <unity.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

// int qos_init(qos_evt_handler_t evt_handler);
// int qos_message_add(struct qos_data *message);
// int qos_message_remove(uint32_t id);
// bool qos_message_has_flag(const struct qos_data *message, uint32_t flag);
// void qos_message_print(const struct qos_data *message);
// uint16_t qos_message_id_get_next(void);
// void qos_message_notify_all(void);
// void qos_message_remove_all(void);
// void qos_timer_reset(void);

void setUp(void)
{
}

void tearDown(void)
{
}

void test_qos_init(void)
{

}

extern int unity_main(void);

void main(void)
{
	(void)unity_main();
}
