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
