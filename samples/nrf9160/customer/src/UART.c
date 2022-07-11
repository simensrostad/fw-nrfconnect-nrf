/**
 * @file        UART.c
 *
 * @details     A module handling the uart.
 *
 * @copyright   Copyright (c) 2021 Circle Consult ApS. All rights reserved.
 *
 * @date        16/09/2020.
 * @author      Anton Vigen Smolarz, Circle Consult ApS.
 */

#include "UART.h"
#include "software_settings.h"

// cppcheck-suppress    unusedFunction
void init_uart(uart_irq_callback_user_data_t a_uart_handler) {

    // Get device binding
    const struct device* uart = device_get_binding(CC_UART_LABEL);

    // Did the device not setup?
    if(NULL == uart) {

        // Inform user.
        printk("Could not set up uart_0\n");
    }

    // Link interrupt requests from the uart to the interrupt handler.
    uart_irq_callback_set(uart, a_uart_handler);

    // Enable RX interrupt requests.
    uart_irq_rx_enable(uart);
}