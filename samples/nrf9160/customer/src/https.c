/**
 * @file        https.c
 *
 * @details     A module handling the HTTPS Client protocol.
 *
 * @copyright   Copyright (c) 2021 Circle Consult ApS. All rights reserved.
 *
 * @date        04-11-2021
 * @author      Christian Friedrichsen, Circle Consult ApS
 */

#include <stdint.h>
#include <stdlib.h>

#include <zephyr.h>
#include <logging/log.h>
#include <net/socket.h>
#include <net/tls_credentials.h>

#include "https.h"
#include "software_settings.h"

#if CC_DEBUG_HTTPS_ENABLED == 1

//! Register the https module to the logging system.
LOG_MODULE_REGISTER(HTTPS, CC_LOG_LEVEL);

//! The client socket file descriptor.
static int socket_fd = 0;

//! The connection established status flag.
static bool connection_established = false;

//! The receive buffer.
static char recv_buffer[CC_HTTPS_RECV_BUF_SIZE] = { 0 };

//! The receive response buffer.
static char recv_resp_buffer[CC_HTTPS_RECV_RESP_BUF_SIZE] = { 0 };

/**
 * @brief   Close the HTTPS client.
 */
static void https_close(void) {

    // Close the client socket file decriptor.
    (void)close(socket_fd);

    connection_established = false;

    LOG_INF("Connection closed.");
}

/**
 * @brief   Open the HTTPS client.
 */
static void https_open(void) {

    // Is the connection already established?
    if(true == connection_established) {

        LOG_ERR("Connecting is already established.");

        return;
    }

    int error;

    LOG_INF("Connecting...");

    // Fill out the initial address info struct.
    struct addrinfo hints = {
        .ai_flags = 0,
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = 0,
    };

    // The resulting address info struct.
    struct addrinfo* result;

    // Convert text strings representing hostnames/IP addresses into a linked list of struct addrinfo structure.
    error = getaddrinfo(CC_HTTPS_IP_ADDRESS, NULL, &hints, &result);

    // Has an error occurred?
    if(0 != error) {

        LOG_ERR("Getaddrinfo failed, error: %d.", errno);

        return;
    }

    // Convert 16-bit value from host to network byte order.
    ((struct sockaddr_in*)result->ai_addr)->sin_port = htons(CC_HTTPS_PORT);

    // Create the socket.
    socket_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TLS_1_2);

    // Has an error occurred?
    if(-1 == socket_fd) {

        LOG_ERR("Opening socket failed.");

        // Free the memory for the result struct.
        freeaddrinfo(result);

        // Close the connection.
        https_close();

        return;
    }

    // Set up TLS peer verification.
    enum {
        NONE = 0,
        OPTIONAL = 1,
        REQUIRED = 2,
    };
    int tls_verify = OPTIONAL;

    // Set the socket options for the TLS verification.
    error = setsockopt(socket_fd, SOL_TLS, TLS_PEER_VERIFY, &tls_verify, sizeof(tls_verify));

    // Has an error occurred?
    if(0 != error) {

        LOG_ERR("Setup peer verification failed, error: %d.", errno);

        // Free the memory for the result struct.
        freeaddrinfo(result);

        // Close the connection.
        https_close();

        return;
    }

    // Connect the socket to the network.
    error = connect(socket_fd, result->ai_addr, sizeof(struct sockaddr_in));

    // Has an error occurred?
    if(0 != error) {

        LOG_ERR("Connection failed, error: %d.", errno);

        // Free the memory for the result struct.
        freeaddrinfo(result);

        // Close the connection.
        https_close();

        return;
    }

    // Free the memory for the result struct.
    freeaddrinfo(result);

    // The connection is established.
    connection_established = true;

    LOG_INF("Connected to %s.", log_strdup(CC_HTTPS_IP_ADDRESS));
}

/**
 * @brief   Send a HTTPS request to the server.
 *
 * @param   request     The request to send.
 */
static void https_send(const char* const request) {

    // Is the connection not established?
    if(false == connection_established) {

        LOG_ERR("Send failed, connection is not established.");

        return;
    }

    // Send the request to the server.
    int bytes = send(socket_fd, request, strlen(request), 0);

    // Has an error occurred while sending?
    if(0 > bytes) {

        LOG_ERR("Send failed, error: %d.", errno);

        // Close the connection.
        https_close();

        return;
    }

    // Print out the transmitted request.
    LOG_INF("Request sent with %d bytes:", bytes);
}

/**
 * @brief   Receive a HTTPS response from the server.
 */
static void https_recv(void) {

    // Is the connection not established?
    if(false == connection_established) {

        LOG_ERR("Receive failed, connection is not established.");

        return;
    }

    // Receive the request from the server.
    int bytes = recv(socket_fd, &recv_buffer, CC_HTTPS_RECV_BUF_SIZE, 0);

    // Has an error occurred while receiving?
    if(0 > bytes) {

        LOG_ERR("Receive failed, error: %d.", errno);

        // Close the connection.
        https_close();

        return;
    }

    // Print out the received response.
    LOG_INF("Response received with %d bytes:", bytes);
}

/**
 * @brief   Retrieve the HTTPS response from the received data.
 *
 * @return  The retrieved response.
 */
static const char* https_resp_retrieve(void) {

    // Retrieve the HTTPS response (searching for the end terminators).
    char* response = strstr(recv_buffer, "\r\n");

    // Has a response successfully been retrieved?
    if(NULL != response) {

        // Calculate the response size.
        size_t size = response - recv_buffer;

        // Add the response to a buffer.
        for(uint32_t i = 0; i < size; i++) {

            recv_resp_buffer[i] = recv_buffer[i];
        }

        // Add a NULL terminator at the end of the response.
        recv_resp_buffer[size + 1] = '\0';
    }

    return recv_resp_buffer;
}

// cppcheck-suppress    unusedFunction
const char* https_open_send_recv_close(const char* const _request) {

    // Open the https connection.
    https_open();

    // Send the https request to the server.
    https_send(_request);

    // Receieve the https response from the server.
    https_recv();

    // Close the https client connection.
    https_close();

    // Retrieve the https response.
    return https_resp_retrieve();
}

#endif