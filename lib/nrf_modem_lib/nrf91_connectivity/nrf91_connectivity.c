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

/* Initialization of nRF Modem Library when enabling CONFIG_NRF_MODEM_LIB_SYS_INIT occurs
 * at the same time as nrf91_connectivity_init is called (at system boot).
 * We don't support this option in order to control the initialization order and handling
 * in this library.
 */
BUILD_ASSERT(!IS_ENABLED(CONFIG_NRF_MODEM_LIB_SYS_INIT), "SYS init of nRF Modem Lib not supported");

/* The nRF91 modem supports both IPv6 and IPv4. */
BUILD_ASSERT((IS_ENABLED(CONFIG_NET_IPV6) && IS_ENABLED(CONFIG_NET_IPV6)),
	     "NET IPv6 and IPv4 support required");

/* Forward declarations */
void pdn_event_handler(uint8_t cid, enum pdn_event event, int reason);

/* Structure used to upkeep internal variables. */
static struct nrf91_connectivity_data {
	struct net_if_conn if_conn;
} ctx = { 0 };

/* Local functions */
static int modem_init(void)
{
	int ret;

	LOG_DBG("Initializing nRF Modem Library");

	ret = nrf_modem_lib_init(NORMAL_MODE);

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

static int init(const struct net_if *iface)
{
	int ret;
	static bool first;

	/* Set default values for network interface's auto option. */
	if (IS_ENABLED(CONFIG_NRF91_CONNECTIVITY_NO_AUTO_START)) {
		net_if_flag_set(iface, NET_IF_NO_AUTO_START);
	}

	if (IS_ENABLED(CONFIG_NRF91_CONNECTIVITY_NO_AUTO_CONNECT)) {
		net_if_flag_set(iface, NET_IF_NO_AUTO_CONNECT);
	}

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

	/* Initialize modem, link controller, and setup handler for default PDP context 0. */
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

	if (!first) {
		/* Only nessecary to register PDN handler once. */
		ret = pdn_default_ctx_cb_reg(pdn_event_handler);
		if (ret) {
			LOG_ERR("pdn_default_ctx_cb_reg, error: %d", ret);
			return ret;
		}

		first = true;
	}

	net_if_dormant_on(iface);

	return 0;
}

static int deinit(void)
{
	int ret;

	ret = lte_lc_deinit();
	if (ret) {
		LOG_ERR("lte_lc_deinit, error: %d", ret);
		return ret;
	}

	return nrf_modem_lib_shutdown();
}

static int enable(void)
{

	return 0;
}

static int disable(void)
{

	return 0;
}

/* Event handlers */
void pdn_event_handler(uint8_t cid, enum pdn_event event, int reason)
{
	ARG_UNUSED(cid);

	switch (event) {
	case PDN_EVENT_CNEC_ESM:
		LOG_DBG("Event: PDP context %d, %s", cid, pdn_esm_strerror(reason));
		break;
	case PDN_EVENT_ACTIVATED:
		LOG_DBG("PDN connection activated");
		LOG_DBG("IPv4 up");

		net_if_dormant_off(ctx.if_conn.iface);

		__ASSERT_NO_MSG(ipv4_addr_add(ctx.if_conn.iface) == 0);

		break;
	case PDN_EVENT_DEACTIVATED:
		LOG_DBG("PDN connection deactivated");

		net_if_dormant_on(ctx.if_conn.iface);

		__ASSERT_NO_MSG(ipv4_addr_remove(ctx.if_conn.iface) == 0);
		__ASSERT_NO_MSG(ipv6_addr_remove(ctx.if_conn.iface) == 0);

		break;
	case PDN_EVENT_IPV6_UP:
		LOG_DBG("IPv6 up");

		__ASSERT_NO_MSG(ipv6_addr_add(ctx.if_conn.iface) == 0);

		break;
	case PDN_EVENT_IPV6_DOWN:
		LOG_DBG("IPv6 down");

		__ASSERT_NO_MSG(ipv6_addr_remove(ctx.if_conn.iface) == 0);

		break;
	default:
		__ASSERT(false, "Unexpected PDN event");
		break;
	}
}

/* Public APIs */
int nrf91_connectivity_init(const struct net_if *iface)
{
	return init(iface);
}

int nrf91_connectivity_enable(const struct net_if *iface, bool enabled)
{
	return enabled ? enable() : disable();
}

int nrf91_connectivity_connect(const struct net_if_conn *if_conn)
{
	ctx.if_conn = *if_conn;

	return lte_lc_func_mode_set(LTE_LC_FUNC_MODE_ACTIVATE_LTE);
}

int nrf91_connectivity_disconnect(const struct net_if_conn *if_conn)
{
	return lte_lc_func_mode_set(LTE_LC_FUNC_MODE_DEACTIVATE_LTE);
}

int nrf91_connectivity_set_opt(const struct net_if_conn *if_conn)
{
	return 0;
}

int nrf91_connectivity_set_opt(const struct net_if_conn *if_conn)
{
	return 0;
}
