#
# Copyright (c) 2019 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
#

set(DEVICE_SHADOW_FOR_AWS_IOT_EMBEDDED_SDK_DIR $ENV{ZEPHYR_BASE}/../modules/lib/device-shadow-for-aws-iot-embedded-sdk)

zephyr_library()
zephyr_library_sources(
	src/aws_iot.c
	${DEVICE_SHADOW_FOR_AWS_IOT_EMBEDDED_SDK_DIR}/source/shadow.c
)

zephyr_include_directories(
	include
	${DEVICE_SHADOW_FOR_AWS_IOT_EMBEDDED_SDK_DIR}/source/include
)
