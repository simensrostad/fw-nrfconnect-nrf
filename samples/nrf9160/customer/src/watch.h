/**
 * @file        watch.h
 *
 * @details     A module handling the watch.
 *
 * @copyright   Copyright (c) 2021 Circle Consult ApS. All rights reserved.
 *
 * @date        09-11-2021
 * @author      Christian Friedrichsen, Circle Consult ApS
 */

#ifndef WATCH_H_
#define WATCH_H_

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief   The watch structure.
 */
typedef struct {

    int64_t unix_time_ms;                       //!< The unix time in milliseconds.
    int64_t unix_time_sec;                      //!< The unix time in seconds.
    int64_t local_time_sec;                     //!< The local time in seconds.
    char unix_time_sec_string[11];              //!< The unix time in seconds as a string.
    char unix_aws_format[64];                   //!< The AWS format.
    char readable_format[64];                   //!< The readable format.

} watch_t;

//! Event identifiers used by the @ref sync_event_callback_t.
typedef enum  {

    SYNC_NONE_EVENT,                            //!< No sync event has occurred.
    SYNC_EVENT_WATCH_UPDATED,                   //!< The watch updated event has occurred.
    SYNC_EVENT_WATCH_FAILED_TO_UPDATE,          //!< The watch failed to update event has occurred.

} sync_event_t;

//! Event identifiers used by the @ref scheduler_event_callback_t.
typedef enum  {

    SCHEDULER_NONE_EVENT,                       //!< No scheduler event has occurred.
    SCHEDULER_EVENT_TICK_OCCURRED,              //!< The scheduler tick event has occurred.
    SCHEDULER_EVENT_INVALID,                    //!< The scheduler invalid event has occurred.

} scheduler_event_t;

//! Synchronization event callback function type.
typedef void (*sync_event_callback_t)(const sync_event_t);

//! Scheduler event callback function type.
typedef void (*scheduler_event_callback_t)(const scheduler_event_t, const watch_t* const);

/**
 * @brief   Initialize the watch.
 */
void watch_init(void);

/**
 * @brief   Set the time offset from UTC including time zone and daylight saving.
 *
 * @param   _offset             Time offset.
 */
void watch_set_offset(const int8_t _offset);

/**
 * @brief   Get the time offset from UTC including time zone and daylight saving.
 *
 * @return  Return the offset.
 */
int8_t watch_get_offset(void);

/**
 * @brief   Update the watch.
 */
void watch_update(void);

/**
 * @brief   Get the current watch.
 *
 * @param   _watch              The watch.
 *
 * @retval  true                The watch is read successfully.
 * @retval  false               The watch failed to be read.
 */
bool watch_get(watch_t* _watch);

/**
 * @brief   Get a timestamp in readable format.
 *
 * @param   _timestamp          The timestamp in Epoch format.
 * @param   _readable_format    The timestamp in readable format.
 * @param   _format_length      The length of the readable format.
 */

void watch_get_readable_format(const int64_t* const _timestamp, char* _readable_format, const uint16_t _format_length);

/**
 * @brief   Set the watch sync callback.
 *
 * @param   _callback           Function to be called when a watch synchronization event has occurred.
 */
void watch_set_sync_callback(const sync_event_callback_t _callback);

/**
 * @brief   Set the scheduler callback.
 *
 * @param   _callback           Function to be called when a scheduler event has occurred.
 */
void watch_set_scheduler_callback(const scheduler_event_callback_t _callback);

#endif /* WATCH_H_ */