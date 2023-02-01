/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NRF91_CONNECTIVITY_H__
#define NRF91_CONNECTIVITY_H__

#include <zephyr/types.h>
#include <zephyr/net/net_l2_connectivity.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Option that specifies which action to take net_if_down() is called.
 *  Handled via net_if_set_conn_opt() and net_if_set_conn_opt().
 *
 *  Default action action can be set via choice symbol CONFIG_NRF91_CONNECTIVITY_NET_DOWN_ACTION.
 *  This option can only be set when the network interface is up.
 */

/** */
#define NRF91_CONNECTIVITY_NET_DOWN_MODEM_SHUTDOWN 1

/** */
#define NRF91_CONNECTIVITY_NET_DOWN_MODEM_SHUTDOWN_IF_NO_GNSS 2

/** */
#define NRF91_CONNECTIVITY_NET_DOWN_LTE_DEACTIVATE 3

/**
 * @brief
 *
 * @details
 */
#define NRF91_LTE_CONN_CTX_TYPE struct nrf91_conn_data

/**
 * @brief
 *
 * @details
 */
struct nrf91_conn_data {
	int dummy;
};

/** @brief
 *
 *  @param
 *
 *  @retval 0 on success.
 */
int nrf91_connectivity_init(const struct net_if *iface, bool enabled);

/** @brief
 *
 *  @param
 *
 *  @retval 0 on success.
 */
int nrf91_connectivity_connect(const struct net_if_conn *if_conn);

/** @brief
 *
 *  @param
 *
 *  @retval 0 on success.
 */
int nrf91_connectivity_disconnect(const struct net_if_conn *if_conn);

#ifdef __cplusplus
}
#endif

/**
 *@}
 */

#endif /* NRF91_CONNECTIVITY_H__ */
