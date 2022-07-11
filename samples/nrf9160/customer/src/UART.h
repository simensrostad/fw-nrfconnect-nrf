/**
 * @file        UART.h
 *
 * @details     A module handling the uart.
 *
 * @copyright   Copyright (c) 2021 Circle Consult ApS. All rights reserved.
 *
 * @date        16/09/2020.
 * @author      Anton Vigen Smolarz, Circle Consult ApS.
 */

#ifndef UART_H
#define UART_H
#include <drivers/uart.h>

/**
 * @brief   Initializes uart.
 *
 * @param   a_uart_handler  The function that should handle the uart.
 */
void init_uart(uart_irq_callback_user_data_t a_uart_handler);

#endif /* UART_H */