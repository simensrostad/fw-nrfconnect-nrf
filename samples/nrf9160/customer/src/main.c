/**
 * Circle Consult ApS
 *
 * Rundforbivej 271A, DK-2850 Naerum
 * Inge Lehmanns Gade 10, DK-8000 Aarhus C
 * Alsion 2, DK-6400 Soenderborg
 *
 * Denmark
 *
 * Phone: +45 45 56 10 56
 * Fax: +45 45 56 11 56
 * Website: http://www.circleconsult.dk
 *
 *  Circle Consult ApS has Intellectual Property Rights.
 *
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are not permitted.
 *
 *  Circle Consult's name may not be used to endorse or promote products derived
 *  from this software without specific prior written permission.
 *
 *
 * DISCLAIMER: THIS SOFTWARE IS PROVIDED BY CIRCLE CONSULT APS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-
 * INFRINGEMENT ARE DISCLAIMED. IN NO EVENT SHALL CIRCLE CONSULT APS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * @file        main.c
 *
 * @details     The main module.
 *
 * @copyright   Copyright (c) 2021 Circle Consult ApS. All rights reserved.
 *
 * @date        16/09/2020
 * @author      Anton Vigen Smolarz, Circle Consult ApS
 */

#include <zephyr.h>
#include <sys/printk.h>
#include <kernel.h>
#include <stdio.h>

#include "cli.h"
#include "software_settings.h"
#include "modem.h"
#include "scheduler.h"
#include "https.h"
#include "cloud.h"
#include "aws.h"
#include "watch.h"
#include "time.h"

/* Still a work in progress. Restructured. */

//! Set up the number of outlets.
outlet_t number_of_outlets = OUTLET_ONE;

/**
 * @brief Setting up the initializing functions.
 */
void configure_system_on_start_up(void) {

    // Initialize client.
    INIT_CLI();

    // Set the modem client id.
    modem_set_client_id();

    // Initialize modem module.
    modem_init();

    // Initialize the cloud.
    cloud_init(modem_get_client_id());

    // Initialize the watch.
    watch_init();

    // Set time offset.
    watch_set_offset(1);

    // Initialize the scheduler.
    scheduler_init();

    // Connect the modem to LTE network.
    modem_connect();

    // Connect to AWS.
    aws_connect();

    // Synchronize the watch.
    watch_update();
    k_sleep(K_MSEC(500));
}

/**
 * @brief The application main entry.
 */
void main(void) {

    // Initialize system.
    configure_system_on_start_up();

    k_sleep(K_FOREVER);

    // // Enter main loop.
    // while(1) {


    // }
}