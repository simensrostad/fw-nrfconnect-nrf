/**
 * @file        modem.c
 *
 * @details     A module handling the modem.
 *
 * @copyright   Copyright (c) 2021 Circle Consult ApS. All rights reserved.
 *
 * @date        14-09-2021
 * @author      Christian Friedrichsen, Circle Consult ApS
 */

#include <stdio.h>
#include <string.h>

#include <zephyr.h>
#include <logging/log.h>
#include <modem/lte_lc.h>
#include <modem/modem_key_mgmt.h>
#include <nrf_modem_at.h>

#include "software_settings.h"
#include "modem.h"

//! Register the modem module to the logging system.
LOG_MODULE_REGISTER(Modem, CC_LOG_LEVEL);

//! The modem connection status flag.
static bool is_connected;

//! An array holding the client id.
static uint8_t client_id[CC_MODEM_CLIENT_ID_LENGTH];

#if CC_MODEM_WRITE_CERTIFICATES == 1

//! Definitions that find the longest certificate.
#define MAX_OF_2            MAX(sizeof(CLOUD_CA_CERTIFICATE), sizeof(CLOUD_CLIENT_PRIVATE_KEY))
#define MAX_LEN             MAX(MAX_OF_2, sizeof(CLOUD_CLIENT_PUBLIC_CERTIFICATE))

void modem_init(void) {

    int error;

    // Turn off the psm mode.
    error = lte_lc_psm_req(false);

    if(error) {

        LOG_ERR("Failed to turn off PSM mode, error: %d.", error);

        return;
    }

    // Turn off the edrx mode.
    error = lte_lc_edrx_req(false);

    if(error) {

        LOG_ERR("Failed to turn off EDRX mode, error: %d.", error);

        return;
    }

    // Initialize the LTE network.
    error = lte_lc_init();

    if(error) {

        LOG_ERR("Failed to initialize the modem, error: %d.", error);

        return;
    }
}

#else

void modem_init(void) {

    int error;

    // Turn off the psm mode.
    error = lte_lc_psm_req(false);

    if(error) {

        LOG_ERR("Failed to turn off PSM mode, error: %d.", error);

        return;
    }

    // Turn off the edrx mode.
    error = lte_lc_edrx_req(false);

    if(error) {

        LOG_ERR("Failed to turn off EDRX mode, error: %d.", error);

        return;
    }

    // Initialize the LTE network.
    error = lte_lc_init();

    if(error) {

        LOG_ERR("Failed to initialize the modem, error: %d.", error);

        return;
    }
}

#endif

/**
 * @brief   Write AT commands to the modem.
 *
 * @param   _at_cmd         The AT command.
 * @param   _at_rsp         The response of the AT command.
 * @param   _at_rsp_length  The expected AT response length.
 */
static void modem_write_at_command(const char* _at_cmd, char* _at_rsp, const uint16_t _at_rsp_length) {

    LOG_INF("AT command send: %s.", _at_cmd);

    // Write AT command and response.
    int error = nrf_modem_at_cmd(_at_rsp, _at_rsp_length, _at_cmd);

    if(error) {

        LOG_INF("AT command sent failed: %d.", error);
    }
}

void modem_connect(void) {

    // Is the connection established?
    if(true == is_connected) {

        return;
    }

    LOG_INF("Connecting...");

    // Connect to the LTE network.
    int error = lte_lc_connect();

    if(error) {

        LOG_ERR("Failed: %d.", error);

        return;
    }

    is_connected = true;

    LOG_INF("Connected");
}

// cppcheck-suppress    unusedFunction
void modem_disconnect(void) {

    // Is the connection not established?
    if(false == is_connected) {

        return;
    }

    LOG_INF("Disconnecting...");

    // Disconnect the LTE network.
    int error = lte_lc_offline();

    if(error) {

        LOG_ERR("Failed: %d.", error);

        return;
    }

    is_connected = false;

    LOG_INF("Disconnected.");
}

void modem_set_client_id(void) {

    // Fetch the IMEI number from the modem.
    char imei_buffer[CC_MODEM_CGSN_RESPONSE_LENGTH];
    modem_write_at_command("AT+CGSN", imei_buffer, sizeof(imei_buffer));

    // Add null terminator to the IMEI number.
    imei_buffer[CC_MODEM_IMEI_LENGTH] = '\0';

    // Create the client id.
    snprintf(client_id, sizeof(client_id), "acdc-%.*s", CC_MODEM_IMEI_LENGTH, imei_buffer);

    LOG_INF("Client ID: %s.", log_strdup(client_id));
}

const uint8_t* modem_get_client_id(void) {

    return client_id;
}