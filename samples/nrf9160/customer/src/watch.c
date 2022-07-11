/**
 * @file        watch.c
 *
 * @details     A module handling the watch.
 *
 * @copyright   Copyright (c) 2021 Circle Consult ApS. All rights reserved.
 *
 * @date        09-11-2021
 * @author      Christian Friedrichsen, Circle Consult ApS
 */

#include <string.h>
#include <stdio.h>
#include <time.h>

#include <zephyr.h>
#include <logging/log.h>
#include <date_time.h>

#include "watch.h"
#include "software_settings.h"

//! Register the watch module to the logging system.
LOG_MODULE_REGISTER(Watch, CC_LOG_LEVEL);

//! The watch scheduler timer structure.
static struct k_timer watch_scheduler_timer;

//! The sync callback function.
static sync_event_callback_t sync_callback = NULL;

//! The scheduler callback function.
static scheduler_event_callback_t scheduler_callback = NULL;

//! The flag indicating if the time is synced.
static bool time_is_synced = false;

//! The time offset including time zone and daylight saving.
static int8_t time_offset = 0;

/**
 * @brief   Get the current date and time in UTC.
 *
 * @param   _timestamp  The fetched timestamp.
 *
 * @retval  true        Timestamp successfully fetched.
 * @retval  false       Timestamp failed to be fetched or is not valid.
 */
static bool watch_now(int64_t* _timestamp) {

    // Is the date and time not valid?
    if(false == date_time_is_valid()) {

        return false;
    }

    // Get the current date and time in UTC.
    int error = date_time_now(_timestamp);

    if(0 != error) {

        LOG_ERR("Achieving date & time failed, error: %d.", error);

        return false;
    }

    // The date and time is valid and updated.
    return true;
}

/**
 * @brief   Handle the date & time events (callback).
 *
 * @param   evt         The event that caused the callback.
 */
static void watch_event_handler(const struct date_time_evt* evt) {

    // Reset the sync event.
    uint8_t event = SYNC_NONE_EVENT;

    // Loop through the obtained date & time event.
    switch(evt->type) {

        case DATE_TIME_OBTAINED_MODEM:
        case DATE_TIME_OBTAINED_NTP:
        case DATE_TIME_OBTAINED_EXT: {

            // Is the obtained timestamp valid?
            int64_t timestamp;

            if(true == watch_now(&timestamp)) {

                LOG_INF("Date & time obtained by: %d", evt->type);

                // Calculate the timestamp to seconds.
                int64_t timestamp_sec = timestamp / 1000;

                // Get the timestamp in readable format.
                watch_t watch;
                watch_get_readable_format(&timestamp_sec,
                                          watch.readable_format,
                                          sizeof(watch.readable_format));

                LOG_INF("Date & time in UTC: %s.", log_strdup(watch.readable_format));

                // Calculate the timestamp remainder up to the CC_WATCH_TIMER_SCHEDULER_WAKEUP_MS.
                uint32_t remainder = (uint32_t)(CC_WATCH_TIMER_SCHEDULER_WAKEUP_MS - (timestamp % CC_WATCH_TIMER_SCHEDULER_WAKEUP_MS));

                // Start the scheduler wakeup timer.
                k_timer_start(&watch_scheduler_timer, K_MSEC(remainder), K_MSEC(CC_WATCH_TIMER_SCHEDULER_WAKEUP_MS));

                // Date and time is updated successfully.
                time_is_synced = true;

                event = SYNC_EVENT_WATCH_UPDATED;
            }

            // Or is it not valid?
            else {

                // Date and time is not updated successfully.
                time_is_synced = false;

                event = SYNC_EVENT_WATCH_FAILED_TO_UPDATE;
            }

            break;
        }

        case DATE_TIME_NOT_OBTAINED:

            LOG_ERR("Date & time NOT obtained.");

            // Start the scheduler wakeup timer.
            k_timer_start(&watch_scheduler_timer, K_MSEC(CC_WATCH_TIMER_SCHEDULER_WAKEUP_MS), K_MSEC(CC_WATCH_TIMER_SCHEDULER_WAKEUP_MS));

            // Date and time failed to update.
            time_is_synced = false;

            event = SYNC_EVENT_WATCH_FAILED_TO_UPDATE;

            break;

        default:

            break;
    }

    // Has a callback been set?
    if((SYNC_NONE_EVENT != event) && (NULL != sync_callback)) {

        // Set the callback function.
        sync_callback(event);
    }
}

/**
 * @brief   Handle the watch scheduler timer.
 *
 * @param   timer   The k_timer structure.
 */
static void watch_scheduler_timer_handler(struct k_timer* timer) {

    ARG_UNUSED(timer);

    // Reset the event.
    uint8_t event = SCHEDULER_NONE_EVENT;

    // Get the current watch.
    watch_t watch;
    bool is_valid = watch_get(&watch);

    // Is the watch valid and synced?
    if((true == is_valid) && (true == time_is_synced)) {

        event = SCHEDULER_EVENT_TICK_OCCURRED;
    }

    // Or is it not valid or synced?
    else {

        event = SCHEDULER_EVENT_INVALID;
    }

    // Has a callback been set?
    if((SCHEDULER_NONE_EVENT != event) && (NULL != scheduler_callback)) {

        // Set the callback function.
        scheduler_callback(event, &watch);
    }
}

void watch_init(void) {

    // Initialize a scheduler wakeup timer.
    k_timer_init(&watch_scheduler_timer, watch_scheduler_timer_handler, NULL);

    time_is_synced = false;
}

void watch_set_offset(const int8_t _time_offset) {

    time_offset = _time_offset;
}

// cppcheck-suppress    unusedFunction
int8_t watch_get_offset(void) {

    return time_offset;
}

void watch_update(void) {

    // Start the scheduler wakeup timer.
    k_timer_start(&watch_scheduler_timer, K_MSEC(CC_WATCH_TIMER_SCHEDULER_WAKEUP_MS), K_MSEC(CC_WATCH_TIMER_SCHEDULER_WAKEUP_MS));

    // Asynchronous update of internal date time in UTC format.
    int error = date_time_update_async(watch_event_handler);

    if(0 != error) {

        LOG_ERR("Updating date & time failed, error: %d.", error);
    }
}

bool watch_get(watch_t* _watch) {

    // Can the watch not be read or is it not synchronized?
    if((false == watch_now(&_watch->unix_time_ms)) || (false == time_is_synced)) {

        LOG_ERR("Date & Time is NOT synchronized.");

        return false;
    }

    struct tm* now;

    // Fetch the unix time in sec.
    _watch->unix_time_sec = _watch->unix_time_ms / 1000;

    // Fetch the milliseconds.
    char msec[6];

    snprintf(msec, sizeof(msec), "%03d", (uint16_t)(_watch->unix_time_ms % 1000));

    // Calculate the local time in seconds.
    _watch->local_time_sec = _watch->unix_time_sec + (time_offset * CC_WATCH_NUMBER_OF_SEC_PER_HOUR);

    // Convert the unix time in seconds to a string.
    snprintf(_watch->unix_time_sec_string, sizeof(_watch->unix_time_sec_string), "%u", (uint32_t)(_watch->unix_time_sec));

    // Get the unix time in AWS format.
    now = gmtime(&_watch->unix_time_sec);

    strftime(_watch->unix_aws_format, sizeof(_watch->unix_aws_format), "%Y-%m-%dT%H:%M:%S.", now);

    strcat(_watch->unix_aws_format, msec);

    strcat(_watch->unix_aws_format, "Z");

    // Get the local time in readable format.
    now = localtime(&_watch->local_time_sec);

    strftime(_watch->readable_format, sizeof(_watch->readable_format), "%A - %Y-%m-%d %H:%M:%S.", now);

    strcat(_watch->readable_format, msec);

    // The watch is synchronized and read successfully.
    return true;
}

void watch_get_readable_format(const int64_t* const _timestamp, char* _readable_format, const uint16_t _format_length) {

    // Get the local time in readable format.
    struct tm* now;

    now = localtime(_timestamp);

    strftime(_readable_format, _format_length, "%A - %Y-%m-%d %H:%M:%S", now);
}

// cppcheck-suppress    unusedFunction
void watch_set_sync_callback(const sync_event_callback_t _callback) {

    // Set the sync callback function.
    sync_callback = _callback;
}

void watch_set_scheduler_callback(const scheduler_event_callback_t _callback) {

    // Set the scheduler callback function.
    scheduler_callback = _callback;
}