#
# Copyright (c) 2023 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

target_sources(app PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/transport.c)

# Add Client ID helper library
add_subdirectory(client_id)

# Make the folder that contains the CA certificate for "test.mosquitto.org" global so that it can
# be located by the MQTT helper library.
#
# The MQTT helper library provisions the certificate
# to the Mbed TLS stack for Native Posix and nRF7002 DK builds.
zephyr_include_directories_ifdef(CONFIG_MQTT_HELPER_PROVISION_CERTIFICATES ../../credentials)
