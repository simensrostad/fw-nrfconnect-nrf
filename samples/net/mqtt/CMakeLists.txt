#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(mqtt)

# Include files that are common for all modules
add_subdirectory(src/common)

# Include mandatory module source folders
add_subdirectory(src/modules/trigger)
add_subdirectory(src/modules/sampler)
add_subdirectory(src/modules/network)
add_subdirectory(src/modules/transport)
add_subdirectory(src/modules/error)

# Optional modules
add_subdirectory_ifdef(CONFIG_MQTT_SAMPLE_LED src/modules/led)
add_subdirectory_ifdef(CONFIG_MQTT_SAMPLE_ENCODER src/modules/encoder)

if (CONFIG_MQTT_SAMPLE_ENCODER)
# Note that here, we're adding CMAKE_SOURCE_DIR to the include path for nanopb.
# This is needed because the below call to nanopb_generate_cpp() is using
# 'RELPATH .'
set(NANOPB_OPTIONS "-I${CMAKE_SOURCE_DIR}")
nanopb_generate_cpp(proto_sources proto_headers RELPATH . src/modules/encoder/protobuf/simple.proto
)

# We need to be able to include generated header files
zephyr_library_include_directories(${CMAKE_CURRENT_BINARY_DIR})

target_sources(app PRIVATE ${proto_sources})

endif()
