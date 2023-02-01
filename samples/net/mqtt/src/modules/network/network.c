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
#include <zephyr/net/conn_mgr_connectivity.h>

#if CONFIG_MODEM_KEY_MGMT
#include "credentials_provision.h"
#endif /* CONFIG_MODEM_KEY_MGMT */
#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(network, CONFIG_MQTT_SAMPLE_NETWORK_LOG_LEVEL);

/* This module does not subscribe to any channels */

#define L4_EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
#define L2_EVENT_MASK (NET_EVENT_CONN_IF_FATAL_ERROR)

static struct net_mgmt_event_callback l4_cb;
static struct net_mgmt_event_callback l2_cb;

static void l4_handler(struct net_mgmt_event_callback *cb, uint32_t event, struct net_if *iface)
{
	int err;
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
		/* Don't care */
		return;
	}

	err = zbus_chan_pub(&NETWORK_CHAN, &status, K_SECONDS(1));
	if (err) {
		LOG_ERR("zbus_chan_pub, error: %d", err);
		SEND_FATAL_ERROR();
	}
}

static void l2_handler(struct net_mgmt_event_callback *cb, uint32_t event, struct net_if *iface)
{
	if (event == NET_EVENT_CONN_IF_FATAL_ERROR) {
		SEND_FATAL_ERROR();
		return;
	}
}

static void network_task(void)
{
	int err;

	net_mgmt_init_event_callback(&l4_cb, l4_handler, L4_EVENT_MASK);
	net_mgmt_add_event_callback(&l4_cb);

	net_mgmt_init_event_callback(&l2_cb, l2_handler, L2_EVENT_MASK);
	net_mgmt_add_event_callback(&l2_cb);

	err = net_if_up(net_if_get_default());
	if (err) {
		LOG_ERR("net_if_get_default, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}

	/* Provision credentials to the modem before the connection is established.
	 * The nRF9160 modem requires that LTE is deactivated when credentials are provisioned.
	 */
#if CONFIG_MODEM_KEY_MGMT
	err = credentials_provision();
	if (err) {
		LOG_ERR("credentials_provision, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
#endif /* CONFIG_MODEM_KEY_MGMT */

	err = conn_mgr_if_connect(net_if_get_default());
	if (err) {
		LOG_ERR("conn_mgr_if_connect, error: %d", err);
		SEND_FATAL_ERROR();
		return;
	}
}

K_THREAD_DEFINE(network_task_id,
		CONFIG_MQTT_SAMPLE_NETWORK_THREAD_STACK_SIZE,
		network_task, NULL, NULL, NULL, 3, 0, 0);
