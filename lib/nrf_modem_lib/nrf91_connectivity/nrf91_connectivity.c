/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 *
 */

#include <zephyr/device.h>
#include <zephyr/types.h>

#include <zephyr/net/offloaded_netdev.h>
#include <zephyr/net/net_l2_connectivity.h>
#include <zephyr/net/socket.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_ip.h>

#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <modem/pdn.h>

#include <zephyr/logging/log.h>

#include "ip_addr_helper.h"

LOG_MODULE_REGISTER(ip_addr_helper, CONFIG_NRF91_CONNECTIVITY_LOG_LEVEL);

/* Semaphore used to indicate when IP is available */
static K_SEM_DEFINE(ip_up_sem, 0, 1);

enum op {
	CONNECT,
	DISCONNECT,
};

struct nrf91_connectivity_msg {
	enum op op;
	const struct net_if_conn *if_conn;
};

static struct nrf91_connectivity_data {
	struct net_if_conn if_conn;
} ctx_data = { 0 };

/* Message queue used to offload tasks to the internal thread. */
K_MSGQ_DEFINE(nrf91_connectivity_queue, sizeof(struct nrf91_connectivity_msg), 5, 4);

/* Local functions */
static int modem_init(void)
{
	int ret;

	LOG_WRN("Initializing nRF Modem Library");

	ret = nrf_modem_lib_init(NORMAL_MODE);

	/* Handle return values related to modem DFU */
	switch (ret) {
	case 0:
		/* Initialization successful, no action required. */
		LOG_WRN("Initialization successful");
		break;
	case MODEM_DFU_RESULT_OK:
		LOG_DBG("Modem DFU successful. The modem will run the updated " \
			"firmware after reinitialization/reboot.");
		break;
	case MODEM_DFU_RESULT_UUID_ERROR:
	case MODEM_DFU_RESULT_AUTH_ERROR:
		LOG_ERR("Modem DFU error: %d. The modem will automatically run the previous " \
			"(non-updated) firmware after reinitialization/reboot.", ret);
		break;
	case MODEM_DFU_RESULT_VOLTAGE_LOW:
		LOG_ERR("Modem DFU not executed due to low voltage, error: %d. " \
			"The modem will retry the update on reboot.", ret);
		break;
	case MODEM_DFU_RESULT_HARDWARE_ERROR:
	case MODEM_DFU_RESULT_INTERNAL_ERROR:
		LOG_ERR("The modem encountered a fatal hardware error during firmware update. ");

		 /* Fall through */
	default:
		/* All non-zero return codes other than DFU result codes are
		 * considered irrecoverable.
		 */
		__ASSERT(false, "nrf_modem_lib_init, error: %d", ret);
		break;
	}

	return ret;
}

/* Event handlers */

/* Handler that is called when the modem hard faults. */
void nrf_modem_fault_handler(struct nrf_modem_fault_info *fault_info)
{
	__ASSERT(false, "Modem error: 0x%x, PC: 0x%x", fault_info->reason,
						       fault_info->program_counter);

	net_mgmt_event_notify(NET_EVENT_IF_CONNECTIVITY_FATAL_ERROR, if_conn->iface);
}

void pdn_event_handler(uint8_t cid, enum pdn_event event, int reason)
{
	ARG_UNUSED(cid);

	switch (event) {
	case PDN_EVENT_CNEC_ESM:
		LOG_WRN("Event: PDP context %d, %s", cid, pdn_esm_strerror(reason));

		/* Send error event here. */
		break;
	case PDN_EVENT_ACTIVATED:
		LOG_WRN("PDN connection activated, IPv4 up");

		__ASSERT_NO_MSG(ip_addr_add(ctx_data.if_conn.iface) == 0);

		break;
	case PDN_EVENT_DEACTIVATED:
		LOG_WRN("PDN connection deactivated");

		__ASSERT_NO_MSG(ip_addr_remove(ctx_data.if_conn.iface) == 0);

		break;
	case PDN_EVENT_IPV6_UP:
		LOG_WRN("PDN_EVENT_IPV6_UP");

		/* Send the right event */
		return;
	case PDN_EVENT_IPV6_DOWN:
		LOG_WRN("PDN_EVENT_IPV6_DOWN");

		/* Send the right event */
		return;
	default:
		LOG_WRN("Unexpected PDN event!");
		break;
	}
}

static void lte_event_handler(const struct lte_lc_evt *const evt)
{
	if ((evt->type == LTE_LC_EVT_MODEM_EVENT) &&
	    (evt->modem_evt == LTE_LC_MODEM_EVT_RESET_LOOP)) {
		LOG_WRN("The modem has detected a reset loop.");
		LOG_WRN("LTE network attach is now "
			"restricted for the next 30 minutes.");
		LOG_WRN("Power-cycle the device to "
			"circumvent this restriction.");
		LOG_WRN("For more information see the nRF91 AT Commands - Command "
			"Reference Guide v2.0 - chpt. 5.36");

		/* Send event that there is connectivity issues going on here. */
	}
}

/* Public APIs */
int nrf91_connectivity_set(const struct net_if *iface, bool enabled)
{
	int ret;

	/* Set default values for persistency and connection timeout */
	ret = net_if_set_conn_timeout((struct net_if *)iface,
				      CONFIG_NRF91_CONNECTIVITY_CONNECT_TIMEOUT_SECONDS);
	if (ret) {
		LOG_ERR("net_if_set_conn_timeout, error: %d", ret);
		return ret;
	}

	ret = net_if_set_conn_persistence((struct net_if *)iface,
					  CONFIG_NRF91_CONNECTIVITY_CONNECTION_PERSISTENCY);
	if (ret) {
		LOG_ERR("net_if_set_conn_persistence, error: %d", ret);
		return ret;
	}

	/* Initialize modem, link controller, and setup handler for default PDN context 0. */
	ret = modem_init();
	if (ret) {
		LOG_ERR("modem_init, error: %d", ret);
		return ret;
	}

	ret = lte_lc_init();
	if (ret) {
		LOG_ERR("lte_lc_init, error: %d", ret);
		return ret;
	}

	/* Subscribe, write why we void this one. */
	(void)lte_lc_modem_events_enable();

	ret = pdn_default_ctx_cb_reg(pdn_event_handler);
	if (ret) {
		LOG_ERR("pdn_default_ctx_cb_reg, error: %d", ret);
		return ret;
	}

	LOG_WRN("nRF91 Connectivity initialized");

	return 0;
}

int nrf91_connectivity_connect(const struct net_if_conn *if_conn)
{
	LOG_WRN("LTE connect requested");

	struct nrf91_connectivity_msg msg = {
		.op = CONNECT,
		.if_conn = if_conn,
	};

	ctx_data.if_conn = *if_conn;

	__ASSERT_NO_MSG(k_msgq_put(&nrf91_connectivity_queue, &msg, K_NO_WAIT) == 0);

	return 0;
}

int nrf91_connectivity_disconnect(const struct net_if_conn *if_conn)
{
	LOG_WRN("LTE disconnect requested");

	struct nrf91_connectivity_msg msg = {
		.op = DISCONNECT,
		.if_conn = if_conn,
	};

	__ASSERT_NO_MSG(k_msgq_put(&nrf91_connectivity_queue, &msg, K_NO_WAIT) == 0);

	return 0;
}

static void on_connect(const struct net_if_conn *if_conn)
{
	LOG_WRN("LTE connect requested");
	// int ret;

	__ASSERT_NO_MSG(lte_lc_connect_async(lte_event_handler) == 0);

	LOG_WRN("Timeout is: %d", if_conn->timeout);
	LOG_WRN("Persistency is: %s", if_conn->persistence ? "Enabled" : "Disabled");

	// ret = k_sem_take(&ip_up_sem, K_SECONDS(if_conn->timeout));
	// if (ret == -EAGAIN) {
	// 	LOG_WRN("Connection timed out");

	// 	/* Connection failed, shutdown LTE. */
	// 	__ASSERT_NO_MSG(lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE) == 0);

	// 	net_mgmt_event_notify(NET_EVENT_IF_CONNECTIVITY_TIMEOUT, if_conn->iface);
	// 	return;
	// } else if (ret == -EBUSY) {
	// 	__ASSERT(false, "Message queue busy, error: %d", ret);
	// }

	// __ASSERT_NO_MSG(ip_addr_add(if_conn->iface) == 0);
}

static void on_disconnect(const struct net_if_conn *if_conn)
{
	__ASSERT_NO_MSG(lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE) == 0);
}

static void nrf91_connectivity_task(void)
{
	struct nrf91_connectivity_msg msg = { 0 };

	while(k_msgq_get(&nrf91_connectivity_queue, (void *)&msg, K_FOREVER) == 0) {

		switch(msg.op) {
		case CONNECT:
			on_connect(msg.if_conn);
			break;
		case DISCONNECT:
			on_disconnect(msg.if_conn);
			break;
		default:
			__ASSERT(false, "Unknown event");
			break;
		}
	}

	__ASSERT(false, "Error getting message queue item");
}

K_THREAD_DEFINE(nrf91_connectivity_id,
		CONFIG_NRF91_CONNECTIVITY_THREAD_STACK_SIZE,
		nrf91_connectivity_task, NULL, NULL, NULL, 3, 0, 0);
