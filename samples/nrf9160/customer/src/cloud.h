/**
 * @file        cloud.h
 *
 * @details     A module handling the Cloud (AWS) communication.
 *
 * @copyright   Copyright (c) 2021 Circle Consult ApS. All rights reserved.
 *
 * @date        22-11-2021
 * @author      Christian Friedrichsen, Circle Consult ApS
 */

#ifndef CLOUD_H_
#define CLOUD_H_

//! Cloud topic types.
typedef enum  {

    CLOUD_TOPIC_TYPE_GET_PCC_LOAD,                  //!< The "Get PCC load" type.
    CLOUD_TOPIC_NUMBER_OF_TYPES                     //!< The number of types.

} cloud_topic_type_t;

//! Event identifiers used by the @ref cloud_event_callback_t.
typedef enum  {

    CLOUD_EVENT_NONE,                               //!< This ID is never used. Dummy value for completeness.
    CLOUD_EVENT_GET_PCC_LOAD_PACKET_SUCCEEDED,      //!< A complete "Get PCC Load" packet has succeeded.
    CLOUD_EVENT_GET_PCC_LOAD_PACKET_FAILED          //!< A complete "Get PCC load" packet has failed.

} cloud_event_t;

/**
 * @brief   Cloud publish information.
 */
typedef struct {

    char* topic;                                    //!< The topic that is published to.
    uint8_t* data;                                  //!< The data to publish.

} cloud_publish_info_t;

//! Event callback function type.
typedef void (*cloud_event_callback_t)(const cloud_event_t);

/**
 * @brief   Initialize the cloud.
 *
 * @param   _client_id      The client id.
 */
void cloud_init(const uint8_t* const _client_id);

/**
 * @brief   Set the callback function.
 *
 * @param   _callback   Function to be called when an event is detected.
 */
void cloud_set_callback(const cloud_event_callback_t _callback);

/**
 * @brief   Create a JSON object so it is ready for publishing.
 *
 * @note    The function acts as an interface handling all different object types defined by @ref cloud_topic_type_t.
 *          The data parameters consist of different data depending of the type (2 set of data arrays are needed for some types).
 *
 * @param   _type                   The packet type.
 * @param   _primary_data           The generic primary data. (Unused = NULL).
 * @param   _primary_data_length    The length of the primary data. (Unused = NULL).
 * @param   _secondary_data         The generic secondary data. (Unused = NULL).
 * @param   _secondary_data_length  The length of the secondary data. (Unused = NULL).
 */
void cloud_create_object(const uint8_t _type,
                         const uint8_t** const _primary_data,
                         const uint16_t* const _primary_data_length,
                         const uint8_t** const _secondary_data,
                         const uint16_t* const _secondary_data_length);

/**
 * @brief   Publish the stored data to the AWS topic.
 */
void cloud_publish(void);

#endif /* CLOUD_H_ */