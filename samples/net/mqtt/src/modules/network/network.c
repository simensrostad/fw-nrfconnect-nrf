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

#if CONFIG_MODEM_KEY_MGMT
#include "credentials_provision.h"
#endif /* CONFIG_MODEM_KEY_MGMT */
#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(network, CONFIG_MQTT_SAMPLE_NETWORK_LOG_LEVEL);

/* This module does not subscribe to any channels */

#define L4_EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)

static struct net_mgmt_event_callback l4_cb;

static void l4_handler(struct net_mgmt_event_callback *cb, uint32_t event, struct net_if *iface)
{
	int ret;
	enum network_status status;

	switch (event) {
	case NET_EVENT_L4_CONNECTED:
		LOG_INF("NET_EVENT_L4_CONNECTED");
		status = NETWORK_CONNECTED;
		break;
	case NET_EVENT_L4_DISCONNECTED:
		LOG_INF("NET_EVENT_L4_DISCONNECTED");
		status = NETWORK_DISCONNECTED;
		break;
	default:
		LOG_WRN("Some unknown event!");
		break;
	}

	ret = zbus_chan_pub(&NETWORK_CHAN, &status, K_SECONDS(1));
	if (ret) {
		LOG_ERR("zbus_chan_pub, error: %d", ret);
		SEND_FATAL_ERROR();
	}
}

static void network_task(void)
{
	int ret;

	net_mgmt_init_event_callback(&l4_cb, l4_handler, L4_EVENT_MASK);
	net_mgmt_add_event_callback(&l4_cb);

	/* Provision credentials to the modem before the connection is established.
	 * The nRF9160 modem requires that LTE is deactivated when credentials are provisioned.
	 */
#if CONFIG_MODEM_KEY_MGMT
	ret = credentials_provision();
	if (ret) {
		LOG_ERR("credentials_provision, error: %d", ret);
		SEND_FATAL_ERROR();
		return;
	}
#endif /* CONFIG_MODEM_KEY_MGMT */

	ret = net_if_connect(net_if_get_default());
	if (ret) {
		LOG_ERR("net_if_connect, error: %d", ret);
		SEND_FATAL_ERROR();
	}
}

K_THREAD_DEFINE(network_task_id,
		CONFIG_MQTT_SAMPLE_NETWORK_THREAD_STACK_SIZE,
		network_task, NULL, NULL, NULL, 3, 0, 0);
