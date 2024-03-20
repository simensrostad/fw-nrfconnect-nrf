/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <errno.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/net/wifi_mgmt.h>
#include <zephyr/net/net_mgmt.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <dk_buttons_and_leds.h>
#include <net/wifi_provision.h>
#include <net/wifi_credentials.h>
#include <zephyr/net/dhcpv4.h>

LOG_MODULE_REGISTER(wifi_provision_sample, CONFIG_WIFI_PROVISION_SAMPLE_LOG_LEVEL);

/* Macro used to subscribe to specific Zephyr NET management events. */
#define L4_EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
#define CONN_LAYER_EVENT_MASK (NET_EVENT_CONN_IF_FATAL_ERROR)

/* Macro called upon a fatal error, reboots the device. */
#define FATAL_ERROR()								\
	LOG_ERR("Fatal error!%s", IS_ENABLED(CONFIG_RESET_ON_FATAL_ERROR) ?	\
				  " Rebooting the device" : "");		\
	LOG_PANIC();								\
	IF_ENABLED(CONFIG_REBOOT, (sys_reboot(0)))

/* Callbacks for Zephyr NET management events. */
static struct net_mgmt_event_callback l4_cb;
static struct net_mgmt_event_callback conn_cb;

/* Semaphore given when the user pushes button 1, used to start provisioning. */
static K_SEM_DEFINE(provision_start_sem, 0, 1);

static void l4_event_handler(struct net_mgmt_event_callback *cb,
			     uint32_t event,
			     struct net_if *iface)
{
	switch (event) {
	case NET_EVENT_L4_CONNECTED:
		LOG_INF("Network connected");

		int ret = dk_set_led_on(DK_LED2);

		if (ret) {
			LOG_ERR("dk_set_led_on, error: %d", ret);
			FATAL_ERROR();
		}

		/* Start DHCPv4 client after provisioned to a network. */
		net_dhcpv4_start(iface);
		break;
	case NET_EVENT_L4_DISCONNECTED:
		LOG_INF("Network disconnected");
		break;
	default:
		/* Don't care */
		return;
	}
}

static void connectivity_event_handler(struct net_mgmt_event_callback *cb,
				       uint32_t event,
				       struct net_if *iface)
{
	if (event == NET_EVENT_CONN_IF_FATAL_ERROR) {
		LOG_ERR("NET_EVENT_CONN_IF_FATAL_ERROR");
		FATAL_ERROR();
		return;
	}
}

/* Callback for Wi-Fi provisioning events. */
static void wifi_provision_handler(const struct wifi_provision_evt *evt)
{
	int ret;

	switch (evt->type) {
	case WIFI_PROVISION_EVT_STARTED:
		LOG_INF("Provisioning started");

		ret = dk_set_led_on(DK_LED1);
		if (ret) {
			LOG_ERR("dk_set_led_on, error: %d", ret);
			FATAL_ERROR();
		}

		break;
	case WIFI_PROVISION_EVT_CLIENT_CONNECTED:
		LOG_INF("Client connected");
		break;
	case WIFI_PROVISION_EVT_CLIENT_DISCONNECTED:
		LOG_INF("Client disconnected");
		break;
	case WIFI_PROVISION_EVT_CREDENTIALS_RECEIVED:
		LOG_INF("Wi-Fi credentials received");
		break;
	case WIFI_PROVISION_EVT_COMPLETED:
		LOG_INF("Provisioning completed");
		break;
	case WIFI_PROVISION_EVT_RESET_REBOOT_REQUEST:
		LOG_INF("Reboot request received, rebooting...");

		LOG_PANIC();
		IF_ENABLED(CONFIG_REBOOT, (sys_reboot(0)));
		break;
	case WIFI_PROVISION_EVT_FATAL_ERROR:
		LOG_ERR("Provisioning failed");
		FATAL_ERROR();
		break;
	default:
		/* Don't care */
		return;
	}
}

static void button_handler(uint32_t button_states, uint32_t has_changed)
{
	if ((has_changed & DK_BTN1_MSK) && (button_states & DK_BTN1_MSK)) {
		LOG_INF("Button 1 pressed");

		/* Give a semaphore instead of calling wifi_provision_start() directly.
		 * This is to offload the aforementioned call to the main thread.
		 * This is needed to not block the button handler thread.
		 */
		k_sem_give(&provision_start_sem);
	}

	if ((has_changed & DK_BTN2_MSK) && (button_states & DK_BTN2_MSK)) {
		LOG_INF("Button 2 pressed, resetting provisioning library");

		int ret = wifi_provision_reset();

		if (ret) {
			LOG_ERR("wifi_provision_reset, error: %d", ret);
			FATAL_ERROR();
			return;
		}
	}
}

static int wifi_power_saving_disable(void)
{
	int ret;
	struct net_if *iface = net_if_get_first_wifi();
		struct wifi_ps_params params = {
		.enabled = WIFI_PS_DISABLED
	};

	ret = net_mgmt(NET_REQUEST_WIFI_PS, iface, &params, sizeof(params));
	if (ret) {
		LOG_ERR("Failed to disable PSM, error: %d", ret);
		FATAL_ERROR();
		return ret;
	}

	return 0;
}

static int wifi_power_saving_enable(void)
{
	int ret;
	struct net_if *iface = net_if_get_first_wifi();
	struct wifi_ps_params params = {
		.enabled = WIFI_PS_ENABLED
	};

	ret = net_mgmt(NET_REQUEST_WIFI_PS, iface, &params, sizeof(params));
	if (ret) {
		LOG_ERR("Failed to enable PSM, error: %d", ret);
		FATAL_ERROR();
		return ret;
	}

	return 0;
}

/* Function used to disable and re-enable PSM after a configured amount of time post provisioning.
 * This is to ensure that the device is discoverable via mDNS so that clients can
 * confirm that provisioning succeeded. This is needed due to m(DNS) SD being unstable
 * in power saving mode.
 */
void psm_set(void)
{
	int ret;

	ret = wifi_power_saving_disable();
	if (ret) {
		LOG_ERR("wifi_power_saving_disable, error: %d", ret);
		FATAL_ERROR();
		return;
	}

	LOG_INF("PSM disabled");

	k_sleep(K_SECONDS(CONFIG_WIFI_PROVISION_SAMPLE_PSM_DISABLED_SECONDS));

	ret = wifi_power_saving_enable();
	if (ret) {
		LOG_ERR("wifi_power_saving_enable, error: %d", ret);
		FATAL_ERROR();
		return;
	}

	LOG_INF("PSM enabled");
}

int main(void)
{
	int ret;
	bool provisioning_completed = false;

	LOG_INF("Wi-Fi provision sample started");

	ret = dk_buttons_init(button_handler);
	if (ret) {
		LOG_ERR("dk_buttons_init, error: %d", ret);
		FATAL_ERROR();
		return ret;
	}

	ret = dk_leds_init();
	if (ret) {
		LOG_ERR("dk_leds_init, error: %d", ret);
		FATAL_ERROR();
		return ret;
	}

	ret = wifi_provision_init(wifi_provision_handler);
	if (ret) {
		LOG_ERR("wifi_provision_init, error: %d", ret);
		FATAL_ERROR();
		return ret;
	}

	ret = conn_mgr_all_if_up(true);
	if (ret) {
		LOG_ERR("conn_mgr_all_if_up, error: %d", ret);
		FATAL_ERROR();
		return ret;
	}

	LOG_INF("Network interface brought up");

	if (wifi_credentials_is_empty()) {
		LOG_INF("Wi-Fi credentials empty, press button 1 to start provisioning");

		k_sem_take(&provision_start_sem, K_FOREVER);

		LOG_INF("Starting provisioning");

		ret = wifi_provision_start();
		if (ret) {
			LOG_ERR("wifi_provision_start, error: %d", ret);
			FATAL_ERROR();
			return ret;
		}

		provisioning_completed = true;

	} else {
		LOG_INF("Wi-Fi credentials found, skipping provisioning");
	}

	/* Register NET mgmt handlers for Connection Manager events after provisioning
	 * has completed. This is to avoid receiving events during provisioning when the device is
	 * in softAP (server) mode and gets assigned an IP,
	 * which triggers the NET_EVENT_L4_CONNECTED.
	 */

	/* Setup handler for Zephyr NET Connection Manager events. */
	net_mgmt_init_event_callback(&l4_cb, l4_event_handler, L4_EVENT_MASK);
	net_mgmt_add_event_callback(&l4_cb);

	/* Setup handler for Zephyr NET Connection Manager Connectivity layer. */
	net_mgmt_init_event_callback(&conn_cb, connectivity_event_handler, CONN_LAYER_EVENT_MASK);
	net_mgmt_add_event_callback(&conn_cb);

	ret = conn_mgr_all_if_connect(true);
	if (ret) {
		LOG_ERR("conn_mgr_all_if_connect, error: %d", ret);
		FATAL_ERROR();
		return ret;
	}

	if (provisioning_completed) {
		psm_set();
	}

	return 0;
}
