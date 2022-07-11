/**
 * @file        software_settings.h
 *
 * @details     A module containing the software settings.
 *
 * @copyright   Copyright (c) 2021 Circle Consult ApS. All rights reserved.
 *
 * @date        12-10-2021
 * @author      Kristian Jensenius Hartmann Jensen, Circle Consult ApS
 */

#ifndef SOFTWARE_SETTINGS_H_
#define SOFTWARE_SETTINGS_H_

#include <devicetree.h>
#include <device.h>
#include <drivers/pwm.h>
#include <zephyr.h>

/**
 * @brief Global definitions.
 */
#define CC_FUNCTION_SUCCESS                         0       //!< Standard function return code for success.
#define CC_WORK_MUTEX_LOCK_TIMEOUT_MS               1000U   //!< The work mutex lock timeout in ms.

//! types of outlet.
typedef enum {
    OUTLET_ERROR    = -1,       //!< When no outlets detected.
    OUTLET_ONE,                 //!< Single outlet.
    OUTLET_TWO,                 //!< Dual outlet.
    MAX_OUTLETS_POSSIBLE        //!< The amount of outlets possible.
} outlet_t;

/**
 * @brief uart_pc
 */
#define CC_UART_LABEL                               DT_LABEL(DT_NODELABEL(uart0))   //!< The PC UART label.
#define CC_PC_COMM_BYTES_MAX                        8                               //!< The maximum number of bytes that a PC packet can fill after the command.
#define CC_UART_PC_RX_BUFFER_SIZE                   16                              //!< The size of the UART RX buffer.
#define CC_UART_PC_TX_BUFFER_SIZE                   16                              //!< The size of the UART TX buffer.
#define CC_UART_PC_MIN_RECEIVED                     7                               //!< The minimum number of bytes to receive, before an UART packet can be valid.
#define CC_UART_PC_CRC16_CHECKSUM_LENGTH            4                               //!< The size of the CRC16 checksum field in nibbles (4 bits).
#define CC_UART_PC_END_TERMINATORS_LENGTH           2                               //!< The size of the end terminators field.

/**
 * @brief hal_adc.c definitions.
 */
#define CC_ADC_LABEL                                DT_LABEL(DT_NODELABEL(adc))     //!< The ADC label.
#define CC_ADC_SAMPLE_TIME_US                       5                               //!< 5 us sample time.
#define CC_ADC_EXTRA_SAMPLES                        99                              //!< Total amount of samples is (1 + extra samples).
#define CC_ADC_RESOLUTION                           10                              //!< ADC resolution.
#define CC_ADC_1_PERCENT_DIFF                       10                              //!< 1% difference at 1023 data resolution.
#define CC_ADC_MAXIMUM_MEASUREMENT                  1023                            //!< How close the datapoints are together.
#define CC_ADC_DATA_SIZE                            100                             //!< Number of extra samples required for 1khz pwm.
#define CC_ADC_PIN_ARGUMENT                         1                               //!< The argument which holds the pin number.

/**
 * @brief hal_gpio.c definitions.
 */
#define CC_GPIO_LABEL                               DT_LABEL(DT_NODELABEL(gpio0))   //!< The GPIO label.
#define CC_GPIO_ACTION_ARGUMENT                     1                               //!< The argument which holds the action the CLI should preform.
#define CC_GPIO_PIN_ARGUMENT                        2                               //!< The argument which holds the pin number.
#define CC_GPIO_DIRECTION_ARGUMENT                  3                               //!< The argument which chooses the direction.
#define CC_GPIO_RESISTOR_ARGUMENT                   4                               //!< The argument which holds the resistor configuration.

/**
 * @brief hal_pwm.c definitions.
 */
#define CC_PWM_OUTLET0_NODE                         DT_NODELABEL(pwm_outlet0)       //!< PWM node 0 label.
#define CC_PWM_OUTLET1_NODE                         DT_NODELABEL(pwm_outlet1)       //!< PWM node 1 label.
#define CC_PWM_DUTY_CYCLE_ARGUMENT                  1                               //!< The argument which holds the duty cycle argument.
#define CC_PWM_OUTLET_ARGUMENT                      2                               //!< The argument which holds the outlet argument.
#define CC_PWM_DUTY_CYCLE_100                       1000                            //!< The value for a 100% duty cycle.

/**
 * @brief watch.c definitions.
 */
#define CC_WATCH_DEBUG_ENABLED                      1                               //!< Enable(1)/disable(0) of printing out the watch in readable format for every scheduled time.
#define CC_WATCH_TIMER_SCHEDULER_WAKEUP_MS          20000                           //!< The number of seconds between each scheduler wakeup (1 hour).
#define CC_WATCH_NUMBER_OF_SEC_PER_HOUR             3600                            //!< The number of seconds in an hour.

/**
 * @brief log definitions.
 */
#define CC_LOG_LEVEL                                3                               //!< The log level used for all LOG_INF/LOG_ERR functions.

/**
 * @brief Modem definitions.
 */
#define CC_MODEM_WRITE_CERTIFICATES                 0                                           //!< Enable(1)/disable(0) the possibility to write certificates manually through the firmware.
#define CC_MODEM_IMEI_LENGTH                        15                                          //!< The length of the IMEI number.
#define CC_MODEM_CGSN_RESPONSE_LENGTH               19 + 6 + 1                                  //!< The length of the CGSN response (Add 6 for CR LF OK CR LF and 1 for \0).
#define CC_MODEM_CLIENT_ID_LENGTH                   (sizeof("acdc-") + CC_MODEM_IMEI_LENGTH)    //!< The length of the client ID.
#define CC_MODEM_CERTIFICATE_SEC_TAG                47353804                                    //!< The security tag used when reading/writing/deleting the certificates in the flash.

/**
 * @brief Watch dog timer definitions.
 */
#define CC_WDT_LABEL                                DT_LABEL(DT_NODELABEL(wdt0))    //!< The Watchdog label.
#define CC_WDT_NO_WINDOW                            0                               //!< Value for not runnning window mode.
#define CC_WDT_TIMEOUT_MS                           10000U                          //!< The Watchdog timeout in ms.
#define CC_WDT_RESET_ON_TIMEOUT                     1                               //!< Reset will happen on timeout.

/**
 * @brief i2c definitions.
 */
#define CC_I2C_LABEL                                DT_LABEL(DT_NODELABEL(i2c2))    //!< The GPIO label.
#define CC_I2C_RECEIVE_SIZE                         1                               //!< The size of the receive message buffer.
#define CC_I2C_ACTION_ARGUMENT                      1                               //!< The argument which holds the action.
#define CC_I2C_ADDRESS_ARGUMENT                     2                               //!< The argument which holds the i2c address.
#define CC_I2C_INSTRUCTION_ARGUMENT                 3                               //!< The argument which holds the send instruction.

/**
 * @brief relay_controller definitions.
 */
#define CC_RELAY_DATA_SIZE                          2                               //!< The data size required for the relay.
#define CC_RELAY_ADDRESS_1                          0x23                            //!< The I2C address for the primary relay.
#define CC_RELAY_ADDRESS_2                          0x27                            //!< The I2C address for the secondary relay.
#define CC_RELAY_RECEIVE_SIZE                       1                               //!< The size received from relay via I2C.
#define CC_RELAY_BIT_NUMBER                         8                               //!< Number of bits in a relay command.
#define CC_RELAY_ACTUATOR_CLOSED                    128                             //!< The minimum value for actuator being closed.

/**
 * @brief peripherals definitions.
 */
#define CC_PERIPHERAL_12V_LOWER                     950                             //!< The lower bounds for a 12V measurement.
#define CC_PERIPHERAL_9V_LOWER                      820                             //!< The lower bounds for a 9V measurement.
#define CC_PERIPHERAL_6V_LOWER                      700                             //!< The lower bounds for a 6V measurement.
#define CC_PERIPHERAL_3V_LOWER                      600                             //!< The lower bounds for a 3V measurement.

#define CC_PERIPHERAL_NO_EV_LOWER                   1000                            //!< The lower bounds for when no EV is plugged in.
#define CC_PERIPHERAL_13A_LOWER                     700                             //!< The lower bounds for a 13A measurement.
#define CC_PERIPHERAL_20A_LOWER                     350                             //!< The lower bounds for a 20A measurement.
#define CC_PERIPHERAL_32A_LOWER                     200                             //!< The lower bounds for a 32A measurement.
#define CC_PERIPHERAL_63A_LOWER                     90                              //!< The lower bounds for a 63A measurement.

/**
 * @brief error definitions.
 */
#define CC_ERROR_TIMEOUT_US                         100000                          //!< The timeout time between trying to fix an error.
#define CC_ERROR_MAX_COUNT                          5                               //!< The maximum amount of time the error handler will run without a fix.

/**
 * @brief cloud definitions.
 */
#define CC_CLOUD_UUID_STRING_LENGTH                 37                              //!< The number of characters for an UUID.
#define CC_CLOUD_THREAD_STACK_SIZE                  8192                            //!< The stack size in bytes used for the cloud thread.
#define CC_CLOUD_COMM_BYTES_MAX                     64                              //!< The maximum communication bytes.

/**
 * @brief AWS definitions
 */
#define CC_AWS_MQTT_BROKER_HOSTNAME                 "a2zs8l7txlw7wc-ats.iot.eu-west-1.amazonaws.com" //!< The AWS MQTT Broker endpoint address.
#define CC_AWS_MQTT_BROKER_PORT                     8883                                            //!< The AWS MQTT Broker secure port.
#define CC_AWS_MQTT_TLS_PEER_VERIFY                 2                                               //!< The AWS MQTT TLS Peer verification.
#define CC_AWS_MQTT_MESSAGE_BUFFER_SIZE             8192                                            //!< The size of the AWS MQTT Message buffer.
#define CC_AWS_MQTT_PAYLOAD_BUFFER_SIZE             8192                                            //!< The size of the AWS MQTT payload buffer.
#define CC_AWS_MQTT_PUB_SUB_STATUS_FAILURE_MESSAGE  "FAILURE"                                       //!< The AWS MQTT status response failure message.
#define CC_AWS_MQTT_PUB_SUB_STATUS_SUCCESS_MESSAGE  "SUCCESS"                                       //!< The AWS MQTT status response success message.
#define CC_AWS_MQTT_PUB_SUB_STATUS_QOS              "1"                                             //!< The AWS MQTT status response QoS (Quality of Service).
#define CC_AWS_MQTT_SNS_MESSAGE_TYPE_ALERT          "alert"                                         //!< The AWS MQTT SNS message type string for the alert.
#define CC_AWS_MQTT_SNS_MESSAGE_TYPE_WARNING        "warning"                                       //!< The AWS MQTT SNS message type string for the warning.

#ifdef USE_CLI

/**
 * @brief Client definitions.
 */
#define CC_CLI_BUFFER_SIZE                          1024                                            //!< The buffer size for the client.
#define CC_CLI_ASCII_UPPER_LIMIT                    127                                             //!< The upper limit for normal ascii symbols.

#ifndef CC_CLI_INITIAL_NUMBER_OF_COMMANDS
#define CC_CLI_INITIAL_NUMBER_OF_COMMANDS           10                                              //!< The initial number of commands for the client.
#endif

#ifndef CC_CLI_RESIZE_CMD_ARRAY_BY
#define CC_CLI_RESIZE_CMD_ARRAY_BY                  10                                              //!< The resize the command array.
#endif

#endif /* USE_CLI */

#endif /* SOFTWARE_SETTINGS_H */
