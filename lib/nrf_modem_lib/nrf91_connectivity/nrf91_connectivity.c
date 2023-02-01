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
#include <zephyr/logging/log.h>
#include <modem/nrf_modem_lib.h>
#include <modem/lte_lc.h>
#include <modem/pdn.h>

#include "ip_addr_helper.h"

LOG_MODULE_REGISTER(nrf91_connectivity, CONFIG_NRF91_CONNECTIVITY_LOG_LEVEL);

/* Initialization of nRF Modem and PDN libraries during SYS_INIT occurs at the same time as
 * nRF91 connectivity is initialized by the network stack (at system boot).
 * We don't support these options in order to control the initialization order in this library.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_NRF_MODEM_LIB_SYS_INIT),
	     "System initialization of nRF Modem library not supported");
BUILD_ASSERT(!IS_ENABLED(CONFIG_PDN_SYS_INIT),
	     "System initialization of PDN library is not supported");

/* The modem supports both IPv6 and IPv4. Therefore we require that the corresponding
 * network stacks are enabled in Zephyr NET.
 */
BUILD_ASSERT(IS_ENABLED(CONFIG_NET_IPV6) && IS_ENABLED(CONFIG_NET_IPV4), "IPv6 and IPv4 required");

/* Forward declarations */
void pdn_event_handler(uint8_t cid, enum pdn_event event, int reason);
static void lte_timeout_work_fn(struct k_work *work);
int nrf91_connectivity_disconnect(const struct net_if_conn *if_conn);

/* Delayable work used to handle LTE connection timeouts. */
static K_WORK_DELAYABLE_DEFINE(lte_timeout_work, lte_timeout_work_fn);

/* Local reference to the network interface the connectivity layer is bound to. */
static struct net_if *iface_bound = NULL;

/* Local functions */

/* Function called when the connection timeout set in the network interface's connectivity
 * structure times out after net_if_connect() as been called.
 */
static void lte_timeout_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);

	LOG_DBG("LTE connection timeout");

	int ret = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE);

	if (ret) {
		LOG_ERR("lte_lc_func_mode_set, error: %d", ret);
		net_mgmt_event_notify(NET_EVENT_IF_CONNECTIVITY_FATAL_ERROR, iface_bound);
		return;
	}

	net_mgmt_event_notify(NET_EVENT_IF_CONNECTIVITY_TIMEOUT, iface_bound);
}

static int modem_init(void)
{
	LOG_DBG("Initializing nRF Modem Library");

	int ret = nrf_modem_lib_init(NORMAL_MODE);

	/* Handle return values related to modem DFU. */
	switch (ret) {
	case 0:
		/* Initialization successful, no action required. */
		return 0;
	case MODEM_DFU_RESULT_OK:
		LOG_DBG("Modem DFU successful. The modem will run the updated " \
			"firmware after reinitialization.");
		break;
	case MODEM_DFU_RESULT_UUID_ERROR:
	case MODEM_DFU_RESULT_AUTH_ERROR:
		LOG_ERR("Modem DFU error: %d. The modem will automatically run the previous " \
			"(non-updated) firmware after reinitialization.", ret);
		break;
	case MODEM_DFU_RESULT_VOLTAGE_LOW:
		LOG_ERR("Modem DFU not executed due to low voltage, error: %d. " \
			"The modem will retry the update on reinitialization.", ret);
		break;
	case MODEM_DFU_RESULT_HARDWARE_ERROR:
	case MODEM_DFU_RESULT_INTERNAL_ERROR:
		 /* Fall through */
	default:
		LOG_ERR("The modem encountered a fatal error during DFU: %d", ret);
		return -ENETDOWN;
	}

	LOG_DBG("Reinitializing nRF Modem Library");

	return nrf_modem_lib_init(NORMAL_MODE);
}

/* Event handlers */
void pdn_event_handler(uint8_t cid, enum pdn_event event, int reason)
{
	ARG_UNUSED(cid);

	int ret;

	switch (event) {
	case PDN_EVENT_CNEC_ESM:
		LOG_DBG("Event: PDP context %d, %s", cid, pdn_esm_strerror(reason));
		break;
	case PDN_EVENT_ACTIVATED:
		LOG_DBG("PDN connection activated");
		LOG_DBG("PDN IPv4 up");

		ret = ipv4_addr_add(iface_bound);
		if (ret) {
			LOG_ERR("ipv4_addr_add, error: %d", ret);
			net_mgmt_event_notify(NET_EVENT_IF_CONNECTIVITY_FATAL_ERROR, iface_bound);
			nrf91_connectivity_disconnect(NULL);
			return;
		}

		net_if_dormant_off(iface_bound);

		/* Cancel ongoing LTE timeout work, if ongoing. */
		k_work_cancel_delayable(&lte_timeout_work);

		break;
	case PDN_EVENT_DEACTIVATED:
		LOG_DBG("PDN connection deactivated");
		LOG_DBG("PDN IPv4 down");

		ret = ipv4_addr_remove(iface_bound);
		if (ret) {
			LOG_ERR("ipv4_addr_remove, error: %d", ret);
			net_mgmt_event_notify(NET_EVENT_IF_CONNECTIVITY_FATAL_ERROR, iface_bound);
			net_if_dormant_on(iface_bound);
			return;
		}

		ret = ipv6_addr_remove(iface_bound);
		if (ret) {
			LOG_ERR("ipv6_addr_remove, error: %d", ret);
			net_mgmt_event_notify(NET_EVENT_IF_CONNECTIVITY_FATAL_ERROR, iface_bound);
			net_if_dormant_on(iface_bound);
			return;
		}

		net_if_dormant_on(iface_bound);

		/* If persistence is disabled, LTE is deactivated upon a lost connection.
		 * Re-establishment is reliant on the application calling net_if_connect()
		 * or net_if_up() if NET_IF_NO_AUTO_CONNECT is disabled to
		 * re-establish the LTE connection.
		 */
		if (!net_if_get_conn_persistence(iface_bound)) {
			ret = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE);
			if (ret) {
				LOG_ERR("lte_lc_func_mode_set, error: %d", ret);
				net_mgmt_event_notify(NET_EVENT_IF_CONNECTIVITY_FATAL_ERROR,
						      iface_bound);
				nrf91_connectivity_disconnect(NULL);
				return;
			}
		}

		break;
	case PDN_EVENT_IPV6_UP:
		LOG_DBG("PDN IPv6 up");

		ret = ipv6_addr_add(iface_bound);
		if (ret) {
			LOG_ERR("ipv6_addr_add, error: %d", ret);
			net_mgmt_event_notify(NET_EVENT_IF_CONNECTIVITY_FATAL_ERROR, iface_bound);
			nrf91_connectivity_disconnect(NULL);
			return;
		}

		break;
	case PDN_EVENT_IPV6_DOWN:
		LOG_DBG("PDN IPv6 down");

		ret = ipv6_addr_remove(iface_bound);
		if (ret) {
			LOG_ERR("ipv6_addr_remove, error: %d", ret);
			net_mgmt_event_notify(NET_EVENT_IF_CONNECTIVITY_FATAL_ERROR, iface_bound);
			net_if_dormant_on(iface_bound);
			return;
		}

		break;
	default:
		LOG_ERR("Unexpected PDN event: %d", event);
		break;
	}
}

/* Public APIs */
void nrf91_connectivity_init(const struct net_if_conn *if_conn)
{
	struct net_if_conn *if_conn_cast = (struct net_if_conn *)if_conn;

	/* Set default values for the network interface's auto options. */
	if (IS_ENABLED(CONFIG_NRF91_CONNECTIVITY_NET_IF_NO_AUTO_START)) {
		net_if_flag_set(if_conn->iface, NET_IF_NO_AUTO_START);
	}

	if (IS_ENABLED(CONFIG_NRF91_CONNECTIVITY_NET_IF_NO_AUTO_CONNECT)) {
		net_if_flag_set(if_conn->iface, NET_IF_NO_AUTO_CONNECT);
	}

	/* Set default values for persistency and connection timeout */
	if_conn_cast->timeout = CONFIG_NRF91_CONNECTIVITY_CONNECT_TIMEOUT_SECONDS;
	if_conn_cast->persistence = IS_ENABLED(CONFIG_NRF91_CONNECTIVITY_CONNECTION_PERSISTENCY);

	net_if_dormant_on(if_conn->iface);

	/* Keep local reference to the network interface the connectivity layer is bound to. */
	iface_bound = if_conn->iface;

	return;
}

int nrf91_connectivity_enable(const struct net_if *iface, bool enabled)
{
	ARG_UNUSED(iface);

	int ret;

	if (!enabled) {
		/* Deactivating LTE, will in turn will make the interface dormant. */
		return nrf91_connectivity_disconnect(NULL);
	}

	if (nrf_modem_is_initialized()) {
		LOG_DBG("nRF Modem library is already initialized");
		return 0;
	}

	/* Initialize modem, PDN, link controller, and setup handler for default PDP context 0. */
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

	pdn_init();

	ret = pdn_default_ctx_cb_reg(pdn_event_handler);
	if (ret) {
		LOG_ERR("pdn_default_ctx_cb_reg, error: %d", ret);
		return ret;
	}

	return 0;
}

int nrf91_connectivity_connect(const struct net_if_conn *if_conn)
{
	ARG_UNUSED(if_conn);

	int ret;

	LOG_DBG("Connecting to LTE...");

	ret = lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_LTE);
	if (ret) {
		LOG_ERR("lte_lc_func_mode_set, error: %d", ret);
		return ret;
	}

	k_work_reschedule(&lte_timeout_work, K_SECONDS(if_conn->timeout));

	LOG_DBG("Connection timeout of %d seconds", if_conn->timeout);
	LOG_DBG("Connection persistency is %s", if_conn->timeout ? "enabled" : "disabled");

	return 0;
}

int nrf91_connectivity_disconnect(const struct net_if_conn *if_conn)
{
	ARG_UNUSED(if_conn);

	/* Cancel ongoing LTE timeout work, if ongoing. */
	k_work_cancel_delayable(&lte_timeout_work);

	return lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE);
}
