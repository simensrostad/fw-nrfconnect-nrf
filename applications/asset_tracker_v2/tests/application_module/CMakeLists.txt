#
# Copyright (c) 2021 Nordic Semiconductor
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(application_module_test)

# Set CMake path variables for convenience
set(ASSET_TRACKER_V2_DIR ../..)

# Generate runner for the test
test_runner_generate(src/application_module_test.c)

# create mock
cmock_handle(../../src/modules/modules_common.h)
cmock_handle(${ZEPHYR_NRF_MODULE_DIR}/include/app_event_manager.h)
cmock_handle(${ZEPHYR_NRF_MODULE_DIR}/subsys/app_event_manager/app_event_manager_priv.h)

cmock_handle(${ZEPHYR_NRF_MODULE_DIR}/include/modem/nrf_modem_lib.h)
cmock_handle(${ZEPHYR_NRFXLIB_MODULE_DIR}/nrf_modem/include/nrf_modem.h)
cmock_handle(${ZEPHYR_NRFXLIB_MODULE_DIR}/nrf_modem/include/nrf_modem_gnss.h)
cmock_handle(${ASSET_TRACKER_V2_DIR}/src/modules/modules_common.h)

# Add debug module (Unit Under Test)
target_sources(app PRIVATE ${ASSET_TRACKER_V2_DIR}/src/main.c)

# Preinclude file to the module under test to redefine IS_ENABLED() macro
# which is used in the module.
set_property(SOURCE ${ASSET_TRACKER_V2_DIR}/src/main.c PROPERTY COMPILE_FLAGS "-Dmain=no_main")

# Add test source file
target_sources(app PRIVATE src/application_module_test.c)

# Include Asset Tracker v2 application folders
target_include_directories(app PRIVATE .)
target_include_directories(app PRIVATE ${ASSET_TRACKER_V2_DIR}/src/)

# Include nRF SDK directories
target_include_directories(app PRIVATE ${ZEPHYR_NRF_MODULE_DIR}/subsys/app_event_manager)
target_include_directories(app PRIVATE ${ZEPHYR_NRF_MODULE_DIR}/modules/cjson/include)

add_compile_definitions(CONFIG_NRF_MODEM_LIB_MEM_DIAG)

target_compile_options(app PRIVATE
  	-DCONFIG_CAF_MODULES_FLAGS_COUNT=32
	-DCONFIG_ASSET_TRACKER_V2_APP_VERSION_MAX_LEN=50
	-DCONFIG_CLOUD_CODEC_APN_LEN_MAX=50
	-DCONFIG_CLOUD_CODEC_LWM2M_PATH_LIST_ENTRIES_MAX=50
	-DCONFIG_LTE_NEIGHBOR_CELLS_MAX=50
	-DCONFIG_MODEM_APN_LEN_MAX=50
	-DCONFIG_APPLICATION_MODULE_LOG_LEVEL=50
)
