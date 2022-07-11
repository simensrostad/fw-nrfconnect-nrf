/**
 * @file        scheduler.c
 *
 * @details     A module handling the scheduler.
 *
 * @copyright   Copyright (c) 2021 Circle Consult ApS. All rights reserved.
 *
 * @date        25-11-2021
 * @author      Christian Friedrichsen, Circle Consult ApS
 */

#include <stdio.h>

#include <zephyr.h>
#include <logging/log.h>

#include "scheduler.h"
#include "software_settings.h"
#include "watch.h"
#include "cloud.h"

//! Register the scheduler module to the logging system.
LOG_MODULE_REGISTER(Scheduler, CC_LOG_LEVEL);

/**
 * @brief   Handle the scheduler events (Callback).
 *
 * @param   event   The scheduler event.
 * @param   watch   The current date and time.
 */
static void scheduler_event_handler(const scheduler_event_t event, const watch_t* const watch) {

    // Look through the event.
    switch(event) {

        // Event SCHEDULER_EVENT_TICK_OCCURRED occurred.
        case SCHEDULER_EVENT_TICK_OCCURRED: {

#if CC_WATCH_DEBUG_ENABLED == 1

            LOG_INF("Date & Time: %s.", log_strdup(watch->readable_format));

#endif

            // Create JSON object and get it published to the topic.
            cloud_create_object(CLOUD_TOPIC_TYPE_GET_PCC_LOAD, NULL, NULL, NULL, NULL);
            cloud_publish();

            break;
        }

        // Event SCHEDULER_EVENT_INVALID occurred.
        case SCHEDULER_EVENT_INVALID:

            LOG_ERR("Date & time is invalid.");

            break;

        default:

            break;
    }
}

void scheduler_init(void) {

    // Set the scheduler callback.
    watch_set_scheduler_callback(scheduler_event_handler);
}