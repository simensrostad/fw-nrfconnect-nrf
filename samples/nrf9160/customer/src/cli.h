/**
 * @file      cli.h
 *
 * @details   A module handling the command line interface.
 *
 * @copyright Copyright (c) 2021 Circle Consult ApS. All rights reserved.
 *
 * @date      16/09/2020
 * @author    Anton Vigen Smolarz, Circle Consult ApS.
 */

#ifndef CLI_H
#define CLI_H

#ifdef USE_CLI

//! Set up the command line interface run type.
typedef void (*CLI_run_t)(void);

/**
 * @brief   Creates a command with its struct to be added later by CLI_ADD_COMMAND.
 *
 * @note    This is a define macro. The internal parts of the macro uses functions and structs already documented.
 *
 * @param   NAME        Name of the command without quotes.
 * @param   DESCRPTION  The commands description as a string in quotes.
 */
#define CLI_CREATE_COMMAND(NAME,DESCRPTION)             \
    void NAME ## _run_cmd_CLI(int argc, char* argv[]);  \
    CLI_command_t NAME ## _cmd_CLI = {                  \
                                                        .name = #NAME,                                  \
                                                        .description = DESCRPTION,                      \
                                                        .run_cmd = & NAME ## _run_cmd_CLI,              \
                                     };                                                  \
    void NAME ## _run_cmd_CLI(int argc, char* argv[])

/**
 * @brief   Adds command to list of available commands.
 *
 * @param   NAME    Name of the command without quotes. Same name as given in CLI_CREATE_COMMAND
 */
#define CLI_ADD_COMMAND(NAME) cli_add_command(& NAME ## _cmd_CLI)

/**
 * @brief Initializes the command line interface.
 */
#define INIT_CLI(void) init_cli()

//! Set up the command line interface command handler type.
typedef void (*CLI_command_handle_t)(int argc, char* argv[]);

//! Set the struct for the command line interface command.
typedef struct CLI_command_t {
    char* name;                     //!< Name of the command.
    char* description;              //!< Description of the command.
    CLI_command_handle_t run_cmd;   //!< command line interface command handler.
} CLI_command_t;

/**
 * @brief   Disables the CLI momentarily.
 */
#define CLI_DISABLE(void) cli_disable()

/**
 * @brief   Enables the CLI after it has been disabled.
 */
#define CLI_ENABLE(void) cli_enable()

/**
 * @brief   Prints the line for writing.
 */
#define CLI_PRINT_LINE(void) cli_print_line()

/**
 * @brief   Function to run command later.
 */
#define CLI_RUN(void) cli_run()

/**
 * @brief   Function to add run command for later run.
 */
#define CLI_ADD_RUN_CMD(CMD) cli_add_run_cmd(CMD)

/**
 * @brief   Initializes the commandline interface.
 */
void init_cli(void);

/**
 * @brief   Take a string and translates it into arguments and runs the matching command.
 *
 * @param   a_line  Pointer to the argument.
 */
void cli_parse(const char* a_line);

/**
 * @brief   Adds command to list of available commands.
 *
 * @param   a_command   Pointer to the command struct.
 */
void cli_add_command(CLI_command_t* a_command);

/**
 * @brief   Disables the CLI momentarily.
 */
void cli_disable(void);

/**
 * @brief   Enables the CLI after it has been disabled.
 */
void cli_enable(void);

/**
 * @brief   Prints the line for writing.
 */
void cli_print_line(void);

/**
 * @brief   Function to run command later.
 */
void cli_run(void);

/**
 * @brief   Function to add run command for later run.
 *
 * @param   cmd     The command to add to run.
 */
void cli_add_run_cmd(CLI_run_t cmd);

#else

/**
 * @brief   This will still create the CLI command, but not its struct since CLI is disabled.
 *
 * @param   NAME        Name of the command without quotes.
 * @param   DESCRPTION  Not used when CLI is disabled.
 */
#define CLI_CREATE_COMMAND(NAME,DESCRPTION) void NAME ## _run_cmd(int argc, char* argv[])

/**
 * @brief   Foo macro that ensures that CLI can be disabled without code change.
 *
 * @param   NAME    Name of the command without quotes.
 */
#define CLI_ADD_COMMAND(NAME)

/**
 * @brief   Foo macro that ensures that CLI can be disabled without code change.
 */
#define INIT_CLI(void)

/**
 * @brief   Foo macro that ensures that CLI can be disabled without code change.
 */
#define CLI_DISABLE(void)

/**
 * @brief   Foo macro that ensures that CLI can be disabled without code change.
 */
#define CLI_ENABLE(void)

/**
 * @brief   Foo macro that ensures that CLI can be disabled without code change.
 */
#define CLI_PRINT_LINE(void)

/**
 * @brief   Function to run command later.
 */
#define CLI_RUN(void)


/**
 * @brief   Function to add run command for later run.
 *
 * @param   CMD     The command to add to run.
 */
#define CLI_ADD_RUN_CMD(CMD)

#endif

#endif /* CLI_H */