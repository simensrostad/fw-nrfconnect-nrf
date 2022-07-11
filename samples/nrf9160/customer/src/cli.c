/**
 * @file      cli.c
 *
 * @details   A module handling the command line interface.
 *
 * @copyright Copyright (c) 2021 Circle Consult ApS. All rights reserved.
 *
 * @date      16/09/2020
 * @author    Anton Vigen Smolarz, Circle Consult ApS.
 */

#ifdef USE_CLI

#include <string.h>
#include <ctype.h>
#include <kernel.h>
#include <sys/reboot.h>
#include <sys/printk.h>
#include <stdbool.h>

#include "cli.h"
#include "uart.h"
#include "software_settings.h"

//! Input buffer.
static uint8_t g_input_buf[CC_CLI_BUFFER_SIZE];

//! Line buffer.
static uint8_t g_linebuf_buf[CC_CLI_BUFFER_SIZE];

//! Set command line interface enable.
static bool g_cli_enabled = false;

//! Set line length.
static uint16_t g_line_length = 0;

//! Set the pointer to the command line interface command.
static CLI_command_t** g_CLI_command_array;

//! Set the number of command line interface commands.
static uint8_t g_CLI_command_num = 0;

//! Set the command line interface command length.
static uint8_t g_CLI_command_length = CC_CLI_INITIAL_NUMBER_OF_COMMANDS;

//! Set the commands added to run.
static bool cmd_is_added_to_run = false;

//! Set the command to run.
static CLI_run_t cmd_to_run;

/**
 * @brief   Command line interface help command, that lists all available commands with there discriptions.
 *
 * @param   argc    Number of arguments. (Not used)
 * @param   argv    value of arguments. (Not used)
 */
CLI_CREATE_COMMAND(help, "Lists all available commands.") {

    // Print header.
    printk("In Circle Consult APS nRF9160 CLI environment\nthe following list of commands are avaliable:");

    // Run through all commands.
    for(uint8_t i = 0; i < g_CLI_command_num; i++) {

        // Print the command name and description.
        printk("\n%s: %s", (*g_CLI_command_array[i]).name, (*g_CLI_command_array[i]).description);
    }
}

/**
 * @brief   Reboot command line interface command, that reboots the system.
 *
 * @param   argc    Number of arguments. (Not used)
 * @param   argv    value of arguments. (Not used)
 */
CLI_CREATE_COMMAND(reboot, "Reboots the devices.") {

    // Call system reboot.
    sys_reboot(0);
}

/**
 * @brief   Gets the number of white space. Double white space is counted only once. Ignores whitespace in quotes.
 *
 * @param   a_string    String witch contains the whitespace.
 *
 * @return              The number of white space.
 */
static uint32_t find_number_of_w_space(const char* a_string) {

    // Initialize the char index.
    uint8_t i = 0;

    // Initialize the return value.
    uint32_t r = 0;

    // Initialize the container that hold whether or not last char was whitespace.
    bool w_space_found = false;

    // Is the character in the string a quote?
    if('"' == a_string[i]) {

        // Increment i.
        i++;

        // While the letters in string is not the end quote.
        while('"' != a_string[i]) {

            // Increment i.
            i++;
        }
    }

    // Run through until end of string.
    while(0 != a_string[i]) {

        // Is the character a whitespace?
        if(0 != isspace((unsigned char)a_string[i])) {

            // Set white space found to true.
            w_space_found = true;
        }

        // If first char of next word is quote, handle as string.
        else if(true == w_space_found && '"' == a_string[i]) {

            // Add one to return value.
            r++;

            // Go to next character.
            i++;

            // Set white space found to false.
            w_space_found = false;

            // Ignore whitespace until next quote or end of string.
            while('"' != a_string[i] && 0 != a_string[i]) {

                // Increment i.
                i++;
            }

            // Is the character the end of the string?
            if(0 == a_string[i]) {
                break;
            }
        }

        // Is there a whitespace found and the next character is not a whitespace to?
        else if(true == w_space_found && 0 == isspace((unsigned char)a_string[i])) {

            // Add one to return value.
            r++;

            // White space found is set to false.
            w_space_found = false;
        }

        // Increment i.
        i++;
    }

    return r;
}

/**
 * @brief   Get the index of the next whitespace.
 *
 * @param   string  The string to find whitespace in.
 * @param   start   The index of where in the string to begin searching.
 *
 * @return          The index of the next whitespace in the string or end of string.
 */
static uint32_t find_white_space(const char* string, const uint32_t start) {

    // Set i as start value.
    uint32_t i = start;

    // While the string is not at the end.
    while(0 != string[i]) {

        // Is there a white space?
        if(0 != isspace((unsigned char)string[i])) {

            return i;
        }

        // Increment i.
        i++;
    }

    return i;
}

/**
 * @brief   Get the index of the next double quote.
 *
 * @param   string  The string to find a quote in.
 * @param   start   The index of where in the string to begin searching.
 *
 * @return          The index of the next whitespace in the string or end of string.
 */
static uint32_t find_quote(const char* string, const uint32_t start) {

    // Set i as the start value.
    uint32_t i = start;

    // While the character is not at the end and carriage return.
    while(0 != string[i] && '\r' != string[i]) {

        // Is the string quotes?
        if('"' == string[i]) {

            return i;
        }

        // Increment i.
        i++;
    }

    return i;
}

/**
 * @brief   Function for handling UART RX interrupts.
 *
 * @param   uart_device     The device that triggered the interrupt.
 * @param   user_data       NA.
 */
static void uart_rx_handler(const struct device* uart_device, void* user_data) {

    // Is command line interface enabled?
    if(true == g_cli_enabled) {

        // Update interrupt request for UART.
        uart_irq_update(uart_device);

        // Is interrupt request for UART RX ready?
        if(uart_irq_rx_ready(uart_device)) {

            // Initialize object for data length.
            uint16_t data_length = 0;

            // Get UART rx data.
            data_length = uart_fifo_read(uart_device, g_input_buf, sizeof(g_input_buf));

            // Ignore control caracters (except carriage return and backspace) and higher values than ascii.
            if(((' ' > g_input_buf[0] && '\r' != g_input_buf[0])) || CC_CLI_ASCII_UPPER_LIMIT < g_input_buf[0]) {

                return;
            }

            // Is the input backspace?
            if(CC_CLI_ASCII_UPPER_LIMIT == g_input_buf[0]) {

                // Does the line have characters?
                if(0 < g_line_length) {

                    // decrement line length.
                    g_line_length--;

                    // Echo backspace back to the sender.
                    printk("%s", g_input_buf);
                }

                return;
            }

            // Run through the input buffer.
            for(uint8_t i = 0; i < data_length; i++) {

                // Add character to line buffer.
                g_linebuf_buf[g_line_length + i] =  g_input_buf[i];
            }

            // Increment line length with the length of the added data.
            g_line_length += data_length;

            // Is the inputbuffer's last character carriage return?
            if('\r' == g_input_buf[data_length - 1]) {

                // Replace carriage return with end of string.
                g_input_buf[data_length - 1] = 0;

                // Replace carriage return with end of string.
                g_linebuf_buf[g_line_length] = 0;

                // Echo received data to user with newline.
                printk("%s\n", g_input_buf);

                // Parse and handle command.
                cli_parse(g_linebuf_buf);

            } else {

                // Add end of string.
                g_input_buf[data_length] = 0;

                // Echo received data to user.
                printk("%s", g_input_buf);
            }
        }
    }
}

void init_cli() {

    // Set command line interface to enabled.
    g_cli_enabled = true;

    // Allocate array space for commands.
    g_CLI_command_array = (CLI_command_t**) k_malloc(sizeof(CLI_command_t*) * g_CLI_command_length);

    // Initialize UART.
    init_uart(uart_rx_handler);

    // Add help command to command line interface.
    CLI_ADD_COMMAND(help);

    // Add reboot command to command line interface.
    CLI_ADD_COMMAND(reboot);

    // Print command line interface header.
    printk("CLI environment started!\nWrite \"help\" for more information.\n> ");
}

void cli_parse(const char* line) {

    // Is the line empty?
    if(1 >= g_line_length) {

        //Reset line to being empty.
        g_line_length = 0;

        // Print indicator to user.
        printk("> ");
        return;
    }

    // Get argument count.
    uint8_t argc = find_number_of_w_space(line) + 1;

    // Allocate argument value list.
    char* argv[argc];

    // Set the start variable.
    uint8_t start = 0;

    // Run through all arguments.
    for(uint8_t i = 0; i < argc; i++) {

        // Allocate indexes for splitting up the line to the arguments.
        static uint8_t stop;
        static uint8_t len;

        // Find next whitespace.
        stop = find_white_space(line, start);

        // Calculate length of argument.
        len = stop - start;

        // Is the length not 0?
        if(0 != len) {

            // Is the character a quote?
            if('"' == line[start]) {

                // Increment start.
                start++;

                // Recalculate length to fit the whole quoted string.
                len = find_quote(line, start) - (start);

                // Recalculated stop.
                stop = start + len;
            }

            // Allocated the correct length of argument.
            argv[i] = (char*) k_malloc(len + 1);

            // Is the argument null?
            if(NULL == argv[i]) {

                // Inform user an error ocurred.
                printk("Error in parsing command\n> ");

                return;
            }

            // Copy value to argument.
            memcpy(argv[i], &(line[start]), len);

            // Terminate the string.
            argv[i][len] = 0;
        }

        // If len is zero then 2 will be added to start on the line below, while only one is wanted in the case of an empty trying. Therefore i--.
        else {

            // Decrement i.
            i--;
        }

        // Set next start to the end + 1.
        start = stop + 1;
    }

    // Initiate variable to check if function ran.
    bool func_ran = false;

    // Do for all command line interface functions.
    for(uint8_t i = 0; i < g_CLI_command_num; i++) {

        // Does the first argument matches command line interface function name?
        if(0 == strcmp((*g_CLI_command_array[i]).name, argv[0])) {

            // Run command line interface function.
            (*g_CLI_command_array[i]).run_cmd(argc, argv);

            // Indicate the function was found and executed.
            func_ran = true;
            break;
        }
    }

    // Has the function been run?
    if(false == func_ran) {

        // Inform user.
        printk("Function not found!");
    }

    // Run through the arguments.
    for(uint8_t i = 0; i < argc; i++) {

        // Free argument.
        k_free(argv[i]);
    }

    // Reset line to being empty.
    g_line_length = 0;

    // Is the command line interface enabled?
    if(true == g_cli_enabled) {

        // Print indicator to user.
        printk("\n> ");
    }
}

void cli_add_command(CLI_command_t* command) {

    // Is the array out of space?
    if(g_CLI_command_num >= g_CLI_command_length) {

        // New array length is calculated.
        uint8_t new_length = CC_CLI_RESIZE_CMD_ARRAY_BY + g_CLI_command_length;

        // Allocate new array with new length.
        CLI_command_t** temp = (CLI_command_t**) k_malloc(sizeof(CLI_command_t*) * new_length);

        // Run through the command array.
        for(uint8_t i = 0; i < g_CLI_command_length; i++) {

            // Move old array to new array.
            temp[i] = g_CLI_command_array[i];
        }

        // Set CLI command length to new length.
        g_CLI_command_length = new_length;

        // Free old array.
        k_free(g_CLI_command_array);

        // Move new pointer to command line interface command array.
        g_CLI_command_array = temp;
    }

    // Is the command array empty?
    if(NULL == g_CLI_command_array) {

        // Inform user.
        printk("CLI not initialized!\n");
    }

    // The command array is not empty.
    else {

        // Add command to array and increment number of commands.
        g_CLI_command_array[g_CLI_command_num++] = command;
    }

}

void cli_disable() {

    // Indicate that command line interface is disabled.
    g_cli_enabled = false;
}

void cli_enable() {

    // Indicate that command line interface is enabled.
    g_cli_enabled = true;
}

void cli_print_line(void) {

    // Set end of string
    g_linebuf_buf[g_line_length] = 0;

    // Print indicator to user.
    printk("\n> %s", g_linebuf_buf);
}

void cli_run(void) {

    // Is a command added to run?
    if(true == cmd_is_added_to_run) {

        // Run command.
        cmd_to_run();

        // Set command added to run to false.
        cmd_is_added_to_run = false;

        // Print command line interface line.
        CLI_PRINT_LINE();
    }
}

void cli_add_run_cmd(CLI_run_t cmd) {

    // Set cmd to run as cmd.
    cmd_to_run = cmd;

    // Set command added to run to true.
    cmd_is_added_to_run = true;
}
#endif