# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.8.2)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

project(customer)

zephyr_include_directories(src)

target_sources(
    app PRIVATE
    src/main.c
    src/watch.c
    src/UART.c
    src/modem.c
    src/https.c
    src/cloud.c
    src/aws.c
    src/scheduler.c)
