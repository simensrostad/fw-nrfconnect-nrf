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

/** Option name for setting/getting the action performed when the network interface is brought down.
 *  Set via the net_if_set_conn_opt() and net_if_get_conn_opt() functions.
 *
 * Default option is NRF91_CONNECTIVITY_NET_IF_DOWN_LTE_DEACTIVATE.
 * This option can only be set when the network interface is up.
 */
#define NRF91_CONNECTIVITY_NET_IF_DOWN_ACTION 1

/** Options values for NRF91_CONNECTIVITY_NET_DOWN_ACTION option. */

/** Deactivate LTE. */
#define NRF91_CONNECTIVITY_NET_IF_DOWN_LTE_DEACTIVATE 1

/** Shutdown the modem completely, regardless of whether GNSS is enabled or not. */
#define NRF91_CONNECTIVITY_NET_IF_DOWN_MODEM_SHUTDOWN 2

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

/** @brief
 *
 *  @param
 *
 *  @retval 0 on success.
 */
int nrf91_connectivity_set_opt(const struct net_if_conn *if_conn,
			       int optname,
			       const void *optval,
			       size_t *optlen);

/** @brief
 *
 *  @param
 *
 *  @retval 0 on success.
 */
int nrf91_connectivity_get_opt(const struct net_if_conn *if_conn,
			       int optname,
			       const void *optval,
			       size_t *optlen);

#ifdef __cplusplus
}
#endif

/**
 *@}
 */

#endif /* NRF91_CONNECTIVITY_H__ */
