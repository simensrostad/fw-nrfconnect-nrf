/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>
#include <zephyr/sys/reboot.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/conn_mgr_connectivity.h>
#include <zephyr/net/conn_mgr_monitor.h>
#include <date_time.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>

#define GPIO_PIN 9

static const struct device *const dev = DEVICE_DT_GET(DT_ALIAS(gpio9));

#if defined(CONFIG_POSIX_API)
#include <zephyr/posix/arpa/inet.h>
#include <zephyr/posix/unistd.h>
#include <zephyr/posix/sys/socket.h>
#endif

LOG_MODULE_REGISTER(udp_sample, CONFIG_UDP_SAMPLE_LOG_LEVEL);

/* Macros used to subscribe to specific Zephyr NET management events. */
#define L4_EVENT_MASK (NET_EVENT_L4_CONNECTED | NET_EVENT_L4_DISCONNECTED)
#define CONN_LAYER_EVENT_MASK (NET_EVENT_CONN_IF_FATAL_ERROR)

/* Macro called upon a fatal error, reboots the device. */
#define FATAL_ERROR()					\
	LOG_ERR("Fatal error! Rebooting the device.");	\
	LOG_PANIC();					\
	IF_ENABLED(CONFIG_REBOOT, (sys_reboot(0)))

/* Zephyr NET management event callback structures. */
static struct net_mgmt_event_callback l4_cb;
static struct net_mgmt_event_callback conn_cb;
static void date_time_event_handler(const struct date_time_evt *evt);
static void getup_work_fn(struct k_work *work);

static K_WORK_DELAYABLE_DEFINE(getup_work, getup_work_fn);

static void getup_work_fn(struct k_work *work)
{
	int ret;

	LOG_INF("It's time to get up!");

	ret = gpio_pin_configure(dev, GPIO_PIN, GPIO_OUTPUT_ACTIVE);
	if (ret < 0) {
		printk("Error: Failed to configure GPIO 9\n");
		return;
	}

	while (1) {
		gpio_pin_toggle(dev, GPIO_PIN);
		k_msleep(1000);
	}

	date_time_update_async(date_time_event_handler);
}

static void obtain_time(void)
{
	int err;
	int64_t date_time;
	struct tm tm;

	err = date_time_now(&date_time);
	if (err) {
		LOG_ERR("date_time_now, error: %d", err);
		FATAL_ERROR();
		return;
	}

	date_time /= MSEC_PER_SEC;

	(void)gmtime_r(&date_time, &tm);

	LOG_INF("Current date and time: %d-%02d-%02d %02d:%02d:%02d",
		tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
		tm.tm_hour, tm.tm_min, tm.tm_sec);

	/* Schedule a timer that triggers at 12:00 */

	/* Calculate the time until 12:00 */
	int64_t time_until_12 = (17 - tm.tm_hour) * 60 * 60 - tm.tm_min * 60 - tm.tm_sec;

	/* Calculate the time until 12:00 the next day */
	if (time_until_12 < 0) {
		time_until_12 += 24 * 60 * 60;
	}

	LOG_WRN("Time until 12:00: %lld seconds", time_until_12);

	getup_work_fn(NULL);
}

static void date_time_event_handler(const struct date_time_evt *evt)
{
	switch (evt->type) {
	case DATE_TIME_OBTAINED_MODEM:
	case DATE_TIME_OBTAINED_NTP:
	case DATE_TIME_OBTAINED_EXT:
		LOG_DBG("DATE_TIME OBTAINED");
		obtain_time();
		break;
	case DATE_TIME_NOT_OBTAINED:
		LOG_INF("DATE_TIME_NOT_OBTAINED");
		break;
	default:
		break;
	}
}

static void on_net_event_l4_connected(void)
{
	date_time_update_async(date_time_event_handler);
}

static void on_net_event_l4_disconnected(void)
{
}
§#
static void l4_event_handler(struct net_mgmt_event_callback *cb,
			     uint32_t event,
			     struct net_if *iface)
{
	switch (event) {
	case NET_EVENT_L4_CONNECTED:
		LOG_INF("Network connectivity established");
		on_net_event_l4_connected();
		break;
	case NET_EVENT_L4_DISCONNECTED:
		LOG_INF("Network connectivity lost");
		on_net_event_l4_disconnected();
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

int main(void)
{
	int err;

	LOG_INF("UDP sample has started");

	/* Setup handler for Zephyr NET Connection Manager events. */
	net_mgmt_init_event_callback(&l4_cb, l4_event_handler, L4_EVENT_MASK);
	net_mgmt_add_event_callback(&l4_cb);

	/* Setup handler for Zephyr NET Connection Manager Connectivity layer. */
	net_mgmt_init_event_callback(&conn_cb, connectivity_event_handler, CONN_LAYER_EVENT_MASK);
	net_mgmt_add_event_callback(&conn_cb);

	/* Connecting to the configured connectivity layer.
	 * Wi-Fi or LTE depending on the board that the sample was built for.
	 */
	LOG_INF("Bringing network interface up and connecting to the network");

	err = conn_mgr_all_if_up(true);
	if (err) {
		LOG_ERR("conn_mgr_all_if_up, error: %d", err);
		FATAL_ERROR();
		return err;
	}

	err = conn_mgr_all_if_connect(true);
	if (err) {
		LOG_ERR("conn_mgr_all_if_connect, error: %d", err);
		FATAL_ERROR();
		return err;
	}

	/* Resend connection status if the sample is built for NATIVE SIM.
	 * This is necessary because the network interface is automatically brought up
	 * at SYS_INIT() before main() is called.
	 * This means that NET_EVENT_L4_CONNECTED fires before the
	 * appropriate handler l4_event_handler() is registered.
	 */
	if (IS_ENABLED(CONFIG_BOARD_NATIVE_SIM)) {
		conn_mgr_mon_resend_status();
	}

	return 0;
}
