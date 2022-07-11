/**
 * @file        aws.c
 *
 * @details     A module handling the AWS & MQTT communication.
 *
 * @copyright   Copyright (c) 2021 Circle Consult ApS. All rights reserved.
 *
 * @date        12-11-2021
 * @author      Christian Friedrichsen, Circle Consult ApS
 */

#include <stdio.h>

#include <zephyr.h>
#include <logging/log.h>
#include <net/mqtt.h>
#include <net/socket.h>
#include <random/rand32.h>

#include "software_settings.h"
#include "aws.h"
#include "watch.h"

//! Register the AWS module to the logging system.
LOG_MODULE_REGISTER(AWS, CC_LOG_LEVEL);

//! The MQTT broker address details structure.
static struct sockaddr_storage mqtt_broker;

//! The MQTT polling file descriptor structure.
static struct pollfd mqtt_pollfd;

//! The MQTT client structure.
static struct mqtt_client client;

//! The MQTT publish mutex structure.
static struct k_mutex aws_publish_mutex;

//! The MQTT publish worker structure.
static struct k_work aws_publish_work;

//! The AWS callback function.
static aws_event_callback_t aws_callback = NULL;

//! The MQTT status when data is published on AWS.
static aws_mqtt_status_t aws_mqtt_status;

//! The MQTT client RX buffer.
static uint8_t mqtt_rx_buffer[CC_AWS_MQTT_MESSAGE_BUFFER_SIZE];

//! The MQTT client TX buffer.
static uint8_t mqtt_tx_buffer[CC_AWS_MQTT_MESSAGE_BUFFER_SIZE];

//! The MQTT payload buffer.
static uint8_t mqtt_payload_buffer[CC_AWS_MQTT_PAYLOAD_BUFFER_SIZE];

//! The MQTT broker connection requested flag.
static atomic_t is_connect_requested;

//! The AWS publish information.
static aws_publish_info_t aws_publish_info;

/**
 * @brief   Publish data to the AWS (Work).
 *
 * @param   work     The k_work structure.
 */
static void aws_publish_work_handler(struct k_work* work) {

    ARG_UNUSED(work);

    // Lock the AWS publish mutex and has it succeeded?
    if(0 != k_mutex_lock(&aws_publish_mutex, K_MSEC(CC_WORK_MUTEX_LOCK_TIMEOUT_MS))) {

        LOG_ERR("AWS publish mutex cannot be locked.");

        return;
    }

    // The parameters for the published message.
    struct mqtt_publish_param mqtt_param;
    memset(&mqtt_param, 0, sizeof(mqtt_param));

    // Fill out the parameters.
    mqtt_param.message.topic.qos = MQTT_QOS_1_AT_LEAST_ONCE;
    mqtt_param.message.topic.topic.utf8 = aws_publish_info.topic;
    mqtt_param.message.topic.topic.size = strlen(aws_publish_info.topic);
    mqtt_param.message.payload.data = aws_publish_info.data;
    mqtt_param.message.payload.len = strlen(aws_publish_info.data);
    mqtt_param.message_id = sys_rand32_get();
    mqtt_param.dup_flag = 0;
    mqtt_param.retain_flag = 0;

    LOG_INF("MQTT published to topic: %s, length: %u.", log_strdup(aws_publish_info.topic), strlen(aws_publish_info.data));

    // Publish the message to the topic.
    int16_t error = mqtt_publish(&client, &mqtt_param);

    if(0 > error) {

        LOG_ERR("Publish data failed: %d.", error);
    }

    // Unlock the AWS publish mutex.
    (void)k_mutex_unlock(&aws_publish_mutex);
}

/**
 * @brief   Get the published MQTT payload.
 *
 * @param   _c          The MQTT client.
 * @param   _length     The length of the payload.
 *
 * @retval  0           Success.
 * @retval  Negative    Error.
 */
static int16_t aws_mqtt_publish_get_payload(struct mqtt_client* const _c, const size_t _length) {

    // Is the published payload too long?
    if(sizeof(mqtt_payload_buffer) < _length) {

        return -EMSGSIZE;
    }

    // Read all the published payload.
    return mqtt_readall_publish_payload(_c, mqtt_payload_buffer, _length);
}

/**
 * @brief   Handle the MQTT client events (Callback).
 *
 * @param   c       The MQTT client.
 * @param   evt     The MQTT event data.
 */
static void aws_mqtt_event_handler(struct mqtt_client* const c, const struct mqtt_evt* evt) {

    int16_t error;

    // Reset the AWS event.
    uint8_t event = AWS_EVENT_NONE;

    // Go through the event type.
    switch(evt->type) {

        // Acknowledgement of connection request.
        case MQTT_EVT_CONNACK:

            // Has an error occurred?
            if(0 != evt->result) {

                LOG_ERR("MQTT connection failed: %d.", evt->result);

                break;
            }

            LOG_INF("MQTT client is connected.");

            // Subscribe the status topic to AWS.
            error = aws_subscribe(aws_mqtt_status.topic);

            // Has an error occurred?
            if(0 > error) {

                LOG_ERR("Subscribe Status failed: %d.", error);
            }

            // Or has it succeeded?
            else {

                LOG_INF("Subscribe Status succeeded.");
            }

            break;

        // Disconnection event.
        case MQTT_EVT_DISCONNECT:

            LOG_INF("MQTT client is disconnected.");

            atomic_set(&is_connect_requested, 0);

            break;

        // Publish event received when message is published on a topic client is subscribed to.
        case MQTT_EVT_PUBLISH: {

            // Read the parameters for the publish message.
            const struct mqtt_publish_param* p = &evt->param.publish;

            LOG_INF("MQTT PUBLISH result = %d, length = %d.", evt->result, p->message.payload.len);

            // Is QoS set to MQTT_QOS_1_AT_LEAST_ONCE?
            if(MQTT_QOS_1_AT_LEAST_ONCE == p->message.topic.qos) {

                // Prepare the QoS1 ack message.
                const struct mqtt_puback_param ack = {

                    .message_id = p->message_id
                };

                // Send the ack message.
                error = mqtt_publish_qos1_ack(&client, &ack);

                // Has an error occurred?
                if(0 != error) {

                    LOG_ERR("MQTT publish QoS1 ack failed: %d.", error);
                }
            }

            // Is QoS set to MQTT_QOS_2_EXACTLY_ONCE?
            else if(MQTT_QOS_2_EXACTLY_ONCE == p->message.topic.qos) {

                // Prepare the QoS2 receive message.
                const struct mqtt_pubrec_param receive = {

                    .message_id = p->message_id
                };

                // Send the receive message.
                error = mqtt_publish_qos2_receive(&client, &receive);

                // Has an error occurred?
                if(0 != error) {

                    LOG_ERR("MQTT publish QoS2 receive failed: %d.", error);
                }
            }

            // Read the published MQTT payload.
            error = aws_mqtt_publish_get_payload(c, p->message.payload.len);

            // Has an error occurred?
            if(0 != error) {

                LOG_ERR("MQTT PUBLISH payload failed: %d.", error);

                break;
            }

            // Has the payload been read successfully?
            LOG_INF("MQTT PUBLISH topic: %s.", log_strdup(p->message.topic.topic.utf8));

            // Has an AWS status topic occurred?
            if(0 == strcmp(p->message.topic.topic.utf8, aws_mqtt_status.topic)) {

                // Set the AWS response ready event.
                event = AWS_EVENT_RESPONSE_READY;
            }

            // Or is the response unknown?
            else {

                LOG_INF("MQTT PUBLISH response is unknown.");
            }

            break;
        }

        // Acknowledgement for published message with QoS 1.
        case MQTT_EVT_PUBACK:

            // Has an error occurred?
            if(0 != evt->result) {

                LOG_ERR("MQTT PUBACK received error: %d.", evt->result);

                break;
            }

            LOG_INF("MQTT PUBACK received with id: %u.", evt->param.puback.message_id);

            break;

        // Reception confirmation for published message with QoS 2.
        case MQTT_EVT_PUBREC: {

            // Has an error occurred?
            if(0 != evt->result) {

                LOG_ERR("MQTT QoS2 PUBREC received error: %d.", evt->result);

                break;
            }

            // Read the parameters for the pubrec message.
            const struct mqtt_pubrec_param* p = &evt->param.pubrec;

            // Prepare the release message.
            const struct mqtt_pubrel_param release = {

                .message_id = p->message_id
            };

            // Send the release message.
            error = mqtt_publish_qos2_release(&client, &release);

            // Has an error occurred?
            if(0 != error) {

                LOG_ERR("MQTT publish QoS2 release failed: %d.", error);

                break;
            }

            LOG_INF("MQTT PUBREC received with id: %u.", p->message_id);

            break;
        }

        // Release of published message with QoS 2.
        case MQTT_EVT_PUBREL: {

            // Has an error occurred?
            if(0 != evt->result) {

                LOG_ERR("MQTT QoS2 PUBREL received error: %d.", evt->result);

                break;
            }

            // Read the parameters for the pubrel message.
            const struct mqtt_pubrel_param* p = &evt->param.pubrel;

            // Prepare the complete message.
            const struct mqtt_pubcomp_param complete = {

                .message_id = p->message_id
            };

            // Send the complete message.
            error = mqtt_publish_qos2_complete(&client, &complete);

            // Has an error occurred?
            if(0 != error) {

                LOG_ERR("MQTT publish QoS2 complete failed: %d.", error);

                break;
            }

            LOG_INF("MQTT PUBREL received with id: %u.", p->message_id);

            break;
        }

        // Confirmation to a publish release message with QoS 2.
        case MQTT_EVT_PUBCOMP: {

            // Has an error occurred?
            if(0 != evt->result) {

                LOG_ERR("MQTT PUBCOMP received error: %d.", evt->result);

                break;
            }

            LOG_INF("MQTT PUBCOMP received with id: %u.", evt->param.pubcomp.message_id);

            break;
        }

        // Acknowledgement to a subscribe request.
        case MQTT_EVT_SUBACK:

            // Has an error occurred?
            if(0 != evt->result) {

                LOG_ERR("MQTT SUBACK received error: %d.", evt->result);

                break;
            }

            LOG_INF("MQTT SUBACK received with id: %u.", evt->param.suback.message_id);

            aws_mqtt_status.is_subscribed = true;

            break;

        // Acknowledgment to a unsubscribe request.
        case MQTT_EVT_UNSUBACK:

            // Has an error occurred?
            if(0 != evt->result) {

                LOG_ERR("MQTT UNSUBACK received error: %d.", evt->result);

                break;
            }

            LOG_INF("MQTT UNSUBACK received with id: %u.", evt->param.unsuback.message_id);

            aws_mqtt_status.is_subscribed = false;

            break;

        // Ping Response from server.
        case MQTT_EVT_PINGRESP:

            break;

        default:

            break;
    }

    // Has a AWS callback been set?
    if((AWS_EVENT_NONE != event) && (NULL != aws_callback)) {

        // Set the callback function.
        aws_callback(event, mqtt_payload_buffer);
    }
}

/**
 * @brief   Initialize the MQTT broker structure.
 */
static void aws_broker_init(void) {

    // Fill out the initial address info struct.
    struct addrinfo hints = {

        .ai_flags = 0,
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = 0,
    };

    // The resulting address info struct.
    struct addrinfo* result;
    struct addrinfo* address;

    // Convert text strings representing hostnames/IP addresses into a linked list of struct addrinfo structure.
    int error = getaddrinfo(CC_AWS_MQTT_BROKER_HOSTNAME, NULL, &hints, &result);

    // Did an error occur?
    if(0 != error) {

        LOG_ERR("Getaddrinfo failed, error: %d.", errno);

        return;
    }

    // Store the resulting address info.
    address = result;

    // Look for the IPv4 Address of the broker.
    while(NULL != address) {

        // Is it an IPv4 Address?
        if(sizeof(struct sockaddr_in) == address->ai_addrlen) {

            // Initialize the broker structure.
            struct sockaddr_in* broker = ((struct sockaddr_in*)&mqtt_broker);

            // Fill out the socket IPv4 address for the broker.
            broker->sin_addr.s_addr = ((struct sockaddr_in*)address->ai_addr)->sin_addr.s_addr;
            broker->sin_family = AF_INET;
            broker->sin_port = htons(CC_AWS_MQTT_BROKER_PORT);

            // An array holding the IP address.
            char ipv4_address[NET_IPV4_ADDR_LEN];

            // Convert the IP address from internal to numeric ASCII form.
            inet_ntop(AF_INET, &broker->sin_addr.s_addr, ipv4_address, sizeof(ipv4_address));

            LOG_INF("MQTT broker is connected with IP address: %s.", log_strdup(ipv4_address));

            break;
        }

        // Increment the address pointer.
        address = address->ai_next;
    }

    // Free the resulting memory struct.
    freeaddrinfo(result);
}

void aws_init(const uint8_t* const _client_id, const aws_event_callback_t _callback) {

    // Set the callback function.
    aws_callback = _callback;

    // Initialize the MQTT client instance.
    mqtt_client_init(&client);

    // MQTT client configuration.
    client.broker = &mqtt_broker;
    client.evt_cb = aws_mqtt_event_handler;
    client.client_id.utf8 = _client_id;
    client.client_id.size = strlen(client.client_id.utf8);
    client.password = NULL;
    client.user_name = NULL;
    client.protocol_version = MQTT_VERSION_3_1_1;

    // MQTT buffers configuration.
    client.rx_buf = mqtt_rx_buffer;
    client.rx_buf_size = sizeof(mqtt_rx_buffer);
    client.tx_buf = mqtt_tx_buffer;
    client.tx_buf_size = sizeof(mqtt_tx_buffer);

    // MQTT transport configuration.
    client.transport.type = MQTT_TRANSPORT_SECURE;

    // The security tag.
    static sec_tag_t sec_tag_list[] = { 128 };

    // TLS configuration for secure MQTT transports.
    struct mqtt_sec_config* tls_config = &(client.transport).tls.config;
    tls_config->peer_verify = CC_AWS_MQTT_TLS_PEER_VERIFY;
    tls_config->cipher_count = 0;
    tls_config->cipher_list = NULL;
    tls_config->sec_tag_count = ARRAY_SIZE(sec_tag_list);
    tls_config->sec_tag_list = sec_tag_list;
    tls_config->hostname = CC_AWS_MQTT_BROKER_HOSTNAME;
    tls_config->session_cache = TLS_SESSION_CACHE_DISABLED;

    // Fill out the MQTT status topic struct.
    aws_mqtt_status.is_subscribed = false;
    aws_mqtt_status.topic = (uint8_t*)_client_id;
    aws_mqtt_status.failure_message = CC_AWS_MQTT_PUB_SUB_STATUS_FAILURE_MESSAGE;
    aws_mqtt_status.success_message = CC_AWS_MQTT_PUB_SUB_STATUS_SUCCESS_MESSAGE;
    aws_mqtt_status.qos = CC_AWS_MQTT_PUB_SUB_STATUS_QOS;

    // Initialize a mutex.
    k_mutex_init(&aws_publish_mutex);

    // Initialize a work queue.
    k_work_init(&aws_publish_work, aws_publish_work_handler);
}

void aws_connect(void) {

    // Is the connection already requested?
    if(1 == atomic_get(&is_connect_requested)) {

        return;
    }

    // Initialize the MQTT broker.
    aws_broker_init();

    // Connect to the MQTT broker.
    int error = mqtt_connect(&client);

    // Has an error occurred?
    if(0 != error) {

        LOG_ERR("MQTT connection failed: %d.", error);

        return;
    }

    // Set the MQTT transport to secure.
    mqtt_pollfd.fd = client.transport.tls.sock;
    mqtt_pollfd.events = POLLIN;

    // The connection is now requested.
    atomic_set(&is_connect_requested, 1);
}

// cppcheck-suppress    unusedFunction
void aws_disconnect(void) {

    // Is the connection not requested?
    if(0 == atomic_get(&is_connect_requested)) {

        return;
    }

    // Disconnect from the MQTT broker.
    int error = mqtt_disconnect(&client);

    // Has an error occurred?
    if(0 != error) {

        LOG_ERR("MQTT disconnection failed: %d.", error);
    }
}

bool aws_is_connection_requested(void) {

    return (bool)atomic_get(&is_connect_requested);
}

void aws_run(void) {

    // Wait on an event on the file descriptor.
    int error = poll(&mqtt_pollfd, 1,  mqtt_keepalive_time_left(&client));

    // Has an error occurred?
    if(0 > error) {

        LOG_ERR("MQTT poll failed: %d.", errno);

        return;
    }

    // Keep the connection alive by sending a ping request.
    error = mqtt_live(&client);

    // Has an error occurred?
    if((0 != error) && (-EAGAIN != error)) {

        LOG_ERR("MQTT live failed: %d.", error);

        return;
    }

    // Is there data to read (returned event)?
    if(POLLIN == (mqtt_pollfd.revents & POLLIN)) {

        // Receive an incoming MQTT packet. The registered callback will be called with the packet content.
        error = mqtt_input(&client);

        // Has an error occurred?
        if(0 != error) {

            LOG_ERR("MQTT input failed: %d.", error);

            return;
        }
    }

    // Does the returned event contain an error condition?
    if(POLLERR == (mqtt_pollfd.revents & POLLERR)) {

        LOG_ERR("MQTT failed with POLLERR.");

        return;
    }

    // Does the returned event contain an invalid request?
    if(POLLNVAL == (mqtt_pollfd.revents & POLLNVAL)) {

        LOG_ERR("MQTT failed with POLLNVAL.");

        return;
    }
}

int16_t aws_subscribe(const char* const _subscribe_topic) {

    // Is the topic already subscribed?
    if(true == aws_mqtt_status.is_subscribed) {

        return 0;
    }

    // Create the subscribed topic.
    struct mqtt_topic topic = {

        .topic = {

            .utf8 = _subscribe_topic,
            .size = strlen(_subscribe_topic)
        },

        .qos = MQTT_QOS_1_AT_LEAST_ONCE
    };

    // Add the topic to the subscription request.
    const struct mqtt_subscription_list subscription_list = {

        .list = &topic,
        .list_count = 1,
        .message_id = sys_rand32_get()
    };

    LOG_INF("MQTT subscribed to topic: %s", log_strdup(_subscribe_topic));

    // Subscribe to the topic.
    return mqtt_subscribe(&client, &subscription_list);
}

void aws_publish(const char* const _topic, const uint8_t* const _data) {

    // Lock the AWS publish mutex and has it succeeded?
    if(0 != k_mutex_lock(&aws_publish_mutex, K_MSEC(CC_WORK_MUTEX_LOCK_TIMEOUT_MS))) {

        LOG_ERR("AWS publish mutex cannot be locked.");

        return;
    }

    // Store the AWS publish information.
    aws_publish_info.topic = (char*)_topic;
    aws_publish_info.data = (uint8_t*)_data;

    // Unlock the AWS publish mutex.
    (void)k_mutex_unlock(&aws_publish_mutex);

    // Submit a work item to the system work queue.
    (void)k_work_submit(&aws_publish_work);
}

void aws_get_status(aws_mqtt_status_t* _aws_mqtt_status) {

    // Get the AWS MQTT status struct.
    memcpy(_aws_mqtt_status, &aws_mqtt_status, sizeof(aws_mqtt_status));
}