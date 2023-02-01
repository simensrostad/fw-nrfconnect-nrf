/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/** @brief default PDP context
 *
 *  @param
 *
 *  @retval 0 on success.
 *  @retval -ENODATA If no IPv4 address was obtained from the modem.
 */
int ipv4_addr_add(const struct net_if *iface);

/** @brief
 *
 *  @param
 *
 *  @retval 0 on success.
 *  @retval -ENODATA If no IPv6 address was obtained from the modem.
 */
int ipv6_addr_add(const struct net_if *iface);

/** @brief
 *
 *  @param
 *
 *  @retval 0 on success.
 */
int ipv4_addr_remove(const struct net_if *iface);

/** @brief
 *
 *  @param
 *
 *  @retval 0 on success.
 */
int ipv6_addr_remove(const struct net_if *iface);
