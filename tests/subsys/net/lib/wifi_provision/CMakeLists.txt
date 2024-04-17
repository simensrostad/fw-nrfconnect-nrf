#
# Copyright (c) 2023 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(wifi_provision_test)

# Generate runner for the test
test_runner_generate(src/wifi_provision_test.c)

# Create mock
cmock_handle(${ZEPHYR_NRF_MODULE_DIR}/include/net/wifi_credentials.h)
cmock_handle(${ZEPHYR_NRF_MODULE_DIR}/include/net/wifi_mgmt_ext.h
        FUNC_EXCLUDE ".*NET_MGMT_DEFINE_REQUEST_HANDLER")
# cmock_handle(${ZEPHYR_NRF_MODULE_DIR}/include/net/aws_fota.h)
# cmock_handle(${ZEPHYR_NRF_MODULE_DIR}/include/dfu/dfu_target.h)

# cmock_handle(${ZEPHYR_BASE}/include/zephyr/net/net_pkt.h)

# Net and Wi-Fi management API mocking requires tricks as the headers have code structures
# that CMock is not able to parse properly.
# cmock_handle(${ZEPHYR_BASE}/include/zephyr/net/net_pkt.h
# 	     WORD_EXCLUDE ".*net_pkt_allocs_cb_t.*")
cmock_handle(${ZEPHYR_BASE}/include/zephyr/net/net_if.h
	     FUNC_EXCLUDE ".*net_if_send_data"
	     FUNC_EXCLUDE ".*net_if_recv_data"
	     FUNC_EXCLUDE ".*net_if_queue_tx"
	     FUNC_EXCLUDE ".*net_if_register_timestamp_cb"
	     FUNC_EXCLUDE ".*net_if_call_timestamp_cb"
	     FUNC_EXCLUDE ".*net_if_add_tx_timestamp"
	     FUNC_EXCLUDE ".*net_if_tx_lock"
	     FUNC_EXCLUDE ".*net_if_unregister_timestamp_cb"
	     WORD_EXCLUDE ".*net_if_timestamp_callback_t"
	     WORD_EXCLUDE ".*struct net_if_timestamp_cb"
	     WORD_EXCLUDE ".*struct net_pkt.*")
cmock_handle(${ZEPHYR_BASE}/include/zephyr/net/net_mgmt.h
	     FUNC_EXCLUDE ".*net_mgmt_event_notify"
	     FUNC_EXCLUDE ".*net_mgmt_event_notify_with_info"
	     FUNC_EXCLUDE ".*net_mgmt_event_wait_on_iface"
	     WORD_EXCLUDE ".*struct net_if.*")
cmock_handle(${ZEPHYR_BASE}/include/zephyr/net/net_mgmt.h
	     FUNC_EXCLUDE ".*net_mgmt_event_notify"
	     FUNC_EXCLUDE ".*net_mgmt_event_notify_with_info"
	     FUNC_EXCLUDE ".*net_mgmt_event_wait_on_iface"
	     WORD_EXCLUDE ".*struct net_if.*")
cmock_handle(${ZEPHYR_BASE}/include/zephyr/net/wifi_mgmt.h
	     FUNC_EXCLUDE "NET_MGMT_DEFINE_REQUEST_HANDLER"
	     FUNC_EXCLUDE "BUILD_ASSERT"
	     FUNC_EXCLUDE "offsetof"
	     FUNC_EXCLUDE ".*wifi_mgmt_raise_disconnect_complete_event"
	     FUNC_EXCLUDE ".*net_mgmt_wifi_event_handler"
       WORD_EXCLUDE ".*struct wifi_raw_scan_result.*"
       FUNC_EXCLUDE ".*wifi_mgmt_raise_raw_scan_result_event.*")

cmock_handle(${ZEPHYR_BASE}/include/zephyr/net/http/parser.h)
cmock_handle(${ZEPHYR_BASE}/include/zephyr/posix/arpa/inet.h)
cmock_handle(${ZEPHYR_BASE}/include/zephyr/posix/unistd.h)
cmock_handle(${ZEPHYR_BASE}/include/zephyr/posix/sys/socket.h)

#include <zephyr/net/tls_credentials.h>
#include <zephyr/net/dns_sd.h>
#include <net/wifi_provision.h>

# # Add Unit Under Test source files
target_sources(app PRIVATE ${ZEPHYR_NRF_MODULE_DIR}/subsys/net/lib/wifi_provision/wifi_provision.c)

# Add test source file
target_sources(app PRIVATE src/wifi_provision_test.c)

# Include paths
target_include_directories(app PRIVATE src)
target_include_directories(app PRIVATE ${ZEPHYR_NRF_MODULE_DIR}/include/net)

# Preinclude file to the UUT to redefine IS_ENABLED().
# set_property(SOURCE ${ZEPHYR_NRF_MODULE_DIR}/subsys/net/lib/wifi_credentials/src/wifi_credentials.c
# 	     PROPERTY COMPILE_FLAGS "-include redefinitions.h")

list(APPEND CMAKE_MODULE_PATH ${ZEPHYR_BASE}/modules/nanopb)
include(nanopb)

set(NANOPB_GENERATE_CPP_STANDALONE OFF)

zephyr_include_directories(app ${ZEPHYR_NRF_MODULE_DIR}/subsys/net/lib/wifi_provision/proto)
zephyr_nanopb_sources(app ${ZEPHYR_NRF_MODULE_DIR}/subsys/net/lib/wifi_provision/proto/common.proto)

# Options that cannot be passed through Kconfig fragments.
target_compile_options(app
  PRIVATE
  -DCONFIG_WIFI_CREDENTIALS_MAX_ENTRIES=2
  -DCONFIG_WIFI_CREDENTIALS_SAE_PASSWORD_LENGTH=128
  -DCONFIG_WIFI_CREDENTIALS_LOG_LEVEL=4
  -DCONFIG_NET_HOSTNAME="test"
  -DCONFIG_WIFI_PROVISION_THREAD_STACK_SIZE=1024
  -DCONFIG_WIFI_PROVISION_CERTIFICATE_SEC_TAG=42
  -DCONFIG_WIFI_PROVISION_TCP_RECV_BUF_SIZE=1024
  -DCONFIG_WIFI_PROVISION_SSID="test"
  -DCONFIG_WIFI_PROVISION_MESSAGE_QUEUE_ENTRIES=5
  -DCONFIG_WIFI_PROVISION_TCP_PORT=443
  -DCONFIG_WIFI_PROVISION_SCAN_RESULT_BUFFER_SIZE=123
  -DCONFIG_WIFI_PROVISION_IPV4_ADDRESS=""
  -DCONFIG_WIFI_PROVISION_IPV4_NETMASK=""
  -DCONFIG_NET_MGMT_EVENT_INFO=y
)
