/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_event.h>

#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(network, CONFIG_MQTT_SAMPLE_NETWORK_LOG_LEVEL);

/* This module does not subscribe to any channels */

#define L4_EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

static struct net_mgmt_event_callback l4_cb;

// extern char *net_sprint_ll_addr_buf(const uint8_t *ll, uint8_t ll_len, char *buf, int buflen);

static void l4_handler(struct net_mgmt_event_callback *cb, uint32_t event, struct net_if *iface)
{
	enum network_status status;

	switch (event) {
	case NET_EVENT_L4_CONNECTED:
		LOG_WRN("NET_EVENT_L4_CONNECTED");
		status = NETWORK_CONNECTED;
		break;
	case NET_EVENT_L4_DISCONNECTED:
		LOG_WRN("NET_EVENT_L4_DISCONNECTED");
		status = NETWORK_DISCONNECTED;
		break;
	default:
		LOG_WRN("Some unknown event!");
		break;
	}

	__ASSERT_NO_MSG(zbus_chan_pub(&NETWORK_CHAN, &status, K_SECONDS(1)) == 0);
}

static void network_task(void)
{
	net_if_set_conn_timeout(net_if_get_default(), 60);
	net_mgmt_init_event_callback(&l4_cb, l4_handler, L4_EVENT_MASK);
	net_mgmt_add_event_callback(&l4_cb);
}

K_THREAD_DEFINE(network_task_id,
		CONFIG_MQTT_SAMPLE_NETWORK_THREAD_STACK_SIZE,
		network_task, NULL, NULL, NULL, 3, 0, 0);
