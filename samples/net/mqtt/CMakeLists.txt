#
# Copyright (c) 2022 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mqtt)

# Include message_channel.h
target_include_directories(app PRIVATE src)

# Include common source files
target_sources(app PRIVATE src/main.c)
target_sources(app PRIVATE src/message_channel.c)

# Include enabled module source folders
add_subdirectory_ifdef(CONFIG_MQTT_SAMPLE_TIMER         src/modules/timer)
add_subdirectory_ifdef(CONFIG_MQTT_SAMPLE_SAMPLER       src/modules/sampler)
add_subdirectory_ifdef(CONFIG_MQTT_SAMPLE_NETWORK       src/modules/network)
add_subdirectory_ifdef(CONFIG_MQTT_SAMPLE_TRANSPORT     src/modules/transport)
