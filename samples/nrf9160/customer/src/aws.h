/**
 * @file        aws.h
 *
 * @details     A module handling the AWS & MQTT communication.
 *
 * @copyright   Copyright (c) 2021 Circle Consult ApS. All rights reserved.
 *
 * @date        12-11-2021
 * @author      Christian Friedrichsen, Circle Consult ApS
 */

#ifndef AWS_H_
#define AWS_H_

#include <stdint.h>
#include <stdbool.h>

#include "software_settings.h"

/**
 * @brief   Subscribe data used when packets are published on AWS.
 */
typedef struct {

    bool is_subscribed;             //!< Flag indicating if the topic is subscribed.
    char* topic;                    //!< The subscribe/publish topic.
    char* failure_message;          //!< The payload message when failed on AWS.
    char* success_message;          //!< The payload message when succeeded on AWS.
    char* qos;                      //!< The Quality of Service.

} aws_mqtt_status_t;

//! Event identifiers used by the @ref aws_event_callback_t.
typedef enum  {

    AWS_EVENT_NONE,                 //!< This ID is never used. Dummy value for completeness.
    AWS_EVENT_RESPONSE_READY,       //!< A complete AWS response packet is ready.

} aws_event_t;

/**
 * @brief   AWS publish information.
 */
typedef struct {

    char* topic;                    //!< The topic that is published to.
    uint8_t* data;                  //!< The data to publish.

} aws_publish_info_t;

//! Cloud event callback function type.
typedef void (*aws_event_callback_t)(const aws_event_t, const uint8_t* const);

/**
 * @brief   Initialize the AWS and MQTT broker.
 *
 * @param   _client_id  The client id.
 * @param   _callback   Function to be called when a aws event is detected.
 */
void aws_init(const uint8_t* const _client_id, const aws_event_callback_t _callback);

/**
 * @brief   Connect to the AWS MQTT broker.
 */
void aws_connect(void);

/**
 * @brief   Disconnect from the AWS MQTT broker.
 */
void aws_disconnect(void);

/**
 * @brief   Verify if the AWS MQTT broker connection is requested.
 *
 * @retval  false   MQTT broker connection is not requested.
 * @retval  true    MQTT broker connection is requested.
 */
bool aws_is_connection_requested(void);

/**
 * @brief   Run for the AWS MQTT broker events and inputs.
 *          This function shall only be called inside a thread.
 */
void aws_run(void);

/**
 * @brief   Subscribe to the configured topic.
 *
 * @param   _subscribe_topic    The topic that shall be subscribed to.
 *
 * @retval  0           Success.
 * @retval  Negative    Error.
 */
int16_t aws_subscribe(const char* const _subscribe_topic);

/**
 * @brief   Publish data on the configured topic.
 *
 * @note    The functionality is handled through a work queue.
 *
 * @param   _topic      The topic that the data shall be published to.
 * @param   _data       The data that shall be published.
 */
void aws_publish(const char* const _topic, const uint8_t* const _data);

/**
 * @brief   Get the MQTT status.
 *
 * @param   _aws_mqtt_status    The MQTT status.
 */
void aws_get_status(aws_mqtt_status_t* _aws_mqtt_status);

#endif /* AWS_H_ */

