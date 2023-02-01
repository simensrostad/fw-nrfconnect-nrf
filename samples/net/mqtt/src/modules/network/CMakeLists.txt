#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

# LTE and Wi-Fi boards
target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/network.c)

# Emulator boards
target_sources_ifdef(CONFIG_BOARD_NATIVE_POSIX
		     app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/network_emulation.c)

# Add Credentials provision library if the Modem key Management API is enabled.
# The library will provision credentials that are placed in the src/credentials/ folder to
# the nRF9160 modem.
add_subdirectory_ifdef(CONFIG_MODEM_KEY_MGMT credentials_provision)
zephyr_include_directories_ifdef(CONFIG_MODEM_KEY_MGMT ../../credentials)
