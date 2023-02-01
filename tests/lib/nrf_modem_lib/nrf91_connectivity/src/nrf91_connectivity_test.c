/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <unity.h>
#include <zephyr/net/net_if.h>
#include <zephyr/net/net_event.h>
#include <zephyr/net/conn_mgr_connectivity.h>

/* Mocked libraries */
#include "cmock_lte_lc.h"
#include "cmock_nrf_modem_lib.h"
#include "cmock_nrf_modem_at.h"
#include "cmock_nrf_modem.h"
#include "cmock_pdn.h"

extern int unity_main(void);

void setUp(void)
{

}

void tearDown(void)
{

}

void test_init_should_set_network_interface_as_dormant(void)
{
	struct net_if *net_if = net_if_get_default();

	TEST_ASSERT_TRUE(net_if_flag_is_set(net_if, NET_IF_DORMANT));
}

void test_init_should_set_timeout(void)
{
	struct net_if *net_if = net_if_get_default();
	int timeout_desired = CONFIG_NRF91_CONNECTIVITY_CONNECT_TIMEOUT_SECONDS;

	TEST_ASSERT_EQUAL(timeout_desired, conn_mgr_if_get_timeout(net_if));
}

// void test_init_should_set_persistency_flag(void)
// {
	// err = conn_mgr_if_connect(net_if_get_default());
	// if (err) {
	// 	return;
	// }
// 	nrf91_connectivity_init();
// }

// void test_init_should_register_pdn_handler(void)
// {
// 	nrf91_connectivity_init();
// }

// void test_init_should_send_fatal_error_upon_pdn_handler_register_failure(void)
// {
// 	nrf91_connectivity_init();
// }

// void test_init_should_send_fatal_error_upon_pdn_handler_register_failure(void)
// {
	// struct net_if *net_if = net_if_get_default();

	// __cmock_nrf_modem_is_initialized_ExpectAndReturn(0);
	// __cmock_nrf_modem_lib_init_ExpectAnyArgsAndReturn(0);
	// __cmock_lte_lc_init_ExpectAndReturn(0);

	// TEST_ASSERT_EQUAL(0, net_if_up(net_if));
// }

void main(void)
{
	(void)unity_main();
}

// void nrf91_connectivity_init(struct conn_mgr_conn_binding * const if_conn);
// int nrf91_connectivity_enable(void);
// int nrf91_connectivity_disable(void);
// int nrf91_connectivity_connect(struct conn_mgr_conn_binding * const if_conn);
// int nrf91_connectivity_disconnect(struct conn_mgr_conn_binding * const if_conn);
// int nrf91_connectivity_options_set(struct conn_mgr_conn_binding * const if_conn,
// 				   int name,
// 				   const void *value,
// 				   size_t length);
// int nrf91_connectivity_options_get(struct conn_mgr_conn_binding * const if_conn,
// 				   int name,
// 				   void *value,
// 				   size_t *length);
