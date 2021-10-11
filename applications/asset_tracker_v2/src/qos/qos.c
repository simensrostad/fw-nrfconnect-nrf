/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>

#include "qos.h"

#include <logging/log.h>
LOG_MODULE_REGISTER(qos, CONFIG_QOS_LOG_LEVEL);

/* Lookup table for backoff reconnection to cloud. Binary scaling.
 *
 * comment: We could provide Kconfig options here to be able to set different retransmission logics
 * 	    - Aggressive retransmission
 *          - Moderate retransmission
 *          - Relaxed retransmissions
 *
 * Alternatively we cloud provide a run time option to switch between retransmission schemas so
 * that this can be controlled depending on network conditions.
 */
struct ack_retrans_delay_lookup {
	uint32_t delay;
} backoff_delay[] = {
	{ 2048 }, { 4096 }, { 8192 }, { 16384 }, { 32768 },
	{ 65536 }, { 131072 }, { 262144 }, { 524288 }, { 1048576 }
};

struct qos_metadata {
	/** Data to be sent to cloud*/
	struct qos_data data;
	/** ID used to uniquely identify the packet. */
	uint16_t id;
	/** Number of times the message has been retransmitted. Used to index backoff delay. */
	uint8_t retry_count;
	/** Timer associated with the message. */
	struct k_timer timer
} qos_internal_list[CONFIG_QOS_INTERNAL_LIST_MAX];

static qos_evt_handler_t app_evt_handler;

static void qos_notify_event(const struct qos_evt *evt)
{
	__ASSERT(evt != NULL, "Library event not found");

	if (app_evt_handler != NULL) {
		app_evt_handler(evt);
	}
}

static void ack_timeout_handler(struct k_timer *timer)
{
	/* I wonder whats acceptable in terms of processing here. */

	/* The k_timer structure provides a pointer to user data.*/
	/* */
}

void qos_init(qos_evt_handler_t evt_handler)
{
	if (evt_handler == NULL) {
		app_evt_handler = NULL;

		LOG_DBG("Previously registered handler %p de-registered",
			app_evt_handler);

		return;
	}

	LOG_DBG("Registering handler %p", evt_handler);

	app_evt_handler = evt_handler;
}

int qos_message_add(const struct qos_data *message)
{
	/* Add message to list with corresponding metadata
	 *
	 * If list if full, iterate the list and replace with a lower priority item that the
	 * ack timer has not been already started for. If timers for all entries is active
	 * take the first low prio item, stop the timer and replace it?
	 *
	 * Propagate event QOS_EVT_MESSAGE_NEW
	 *
	 * Start associated timer
	 *
	 */

}

int qos_message_acked(const struct qos_data *message)
{
	/* Remove message from list
	 *
	 * Stop timer associated timer
	 * Propagate event QOS_EVT_MESSAGE_ACKED
	 *
	 */
}

int qos_message_remove(const struct qos_data *message)
{
	/* Remove message from list
	 *
	 * Stop timer associated timer
	 * Propagate event QOS_EVT_MESSAGE_REMOVED_FROM_LIST
	 *
	 */
}
