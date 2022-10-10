/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdlib.h>
#include <modem/lte_lc.h>
#include <net/aws_iot.h>
#include <modem/modem_key_mgmt.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(aws_iot_provision, CONFIG_AWS_IOT_PROVISION_LOG_LEVEL);

/* Connect work called after provisioning. */
static struct k_work_delayable connect_after_provisioning_work;

/* Flag set when device has received and provisioned all its credentials. */
static bool device_provisioned;

/* Semaphore used to postpone getting credentials until connected to AWS IoT. */
static K_SEM_DEFINE(cloud_connected, 0, 1);

/* Security tag used to write new credentials. */
nrf_sec_tag_t sec_tag = 50;

/* CA, needed in addition to the private key and client certificate. */
#define CA								\
"-----BEGIN CERTIFICATE-----\n"						\
"MIIDQTCCAimgAwIBAgITBmyfz5m/jAo54vB4ikPmljZbyjANBgkqhkiG9w0BAQsF\n"	\
"ADA5MQswCQYDVQQGEwJVUzEPMA0GA1UEChMGQW1hem9uMRkwFwYDVQQDExBBbWF6\n"	\
"b24gUm9vdCBDQSAxMB4XDTE1MDUyNjAwMDAwMFoXDTM4MDExNzAwMDAwMFowOTEL\n"	\
"MAkGA1UEBhMCVVMxDzANBgNVBAoTBkFtYXpvbjEZMBcGA1UEAxMQQW1hem9uIFJv\n"	\
"b3QgQ0EgMTCCASIwDQYJKoZIhvcNAQEBBQADggEPADCCAQoCggEBALJ4gHHKeNXj\n"	\
"ca9HgFB0fW7Y14h29Jlo91ghYPl0hAEvrAIthtOgQ3pOsqTQNroBvo3bSMgHFzZM\n"	\
"9O6II8c+6zf1tRn4SWiw3te5djgdYZ6k/oI2peVKVuRF4fn9tBb6dNqcmzU5L/qw\n"	\
"IFAGbHrQgLKm+a/sRxmPUDgH3KKHOVj4utWp+UhnMJbulHheb4mjUcAwhmahRWa6\n"	\
"VOujw5H5SNz/0egwLX0tdHA114gk957EWW67c4cX8jJGKLhD+rcdqsq08p8kDi1L\n"	\
"93FcXmn/6pUCyziKrlA4b9v7LWIbxcceVOF34GfID5yHI9Y/QCB/IIDEgEw+OyQm\n"	\
"jgSubJrIqg0CAwEAAaNCMEAwDwYDVR0TAQH/BAUwAwEB/zAOBgNVHQ8BAf8EBAMC\n"	\
"AYYwHQYDVR0OBBYEFIQYzIU07LwMlJQuCFmcx7IQTgoIMA0GCSqGSIb3DQEBCwUA\n"	\
"A4IBAQCY8jdaQZChGsV2USggNiMOruYou6r4lK5IpDB/G/wkjUu0yKGX9rbxenDI\n"	\
"U5PMCCjjmCXPI6T53iHTfIUJrU6adTrCC2qJeHZERxhlbI1Bjjt/msv0tadQ1wUs\n"	\
"N+gDS63pYaACbvXy8MWy7Vu33PqUXHeeE6V/Uq2V8viTO96LXFvKWlJbYK8U90vv\n"	\
"o/ufQJVtMVT8QtPHRh8jrdkPSHCa2XV4cdFyQzR1bldZwgJcJmApzyMZFo6IQ6XU\n"	\
"5MsI+yMRQ+hDKXJioaldXgjUkK642M4UwtBV8ob2xJNDd2ZhwLnoQdeXeGADbkpy\n"	\
"rqXRfboQnoZsG4q5WTP468SQvvG5\n"					\
"-----END CERTIFICATE-----\n"

/* Strings used to filter data on incoming topics. */
#define KEY_TOPIC_FILTER "/accepted/key"
#define CERT_TOPIC_FILTER "/accepted/cert"

/* Topic subscribed to to get credentials on:
 * certificate/${deviceId}/create/accepted/key
 * certificate/${deviceId}/create/accepted/cert
 */
static char key_cert_topic[75] = "certificate/" CONFIG_AWS_IOT_CLIENT_ID_STATIC "/create/accepted/+";

/* Topic that is published to in order to get new credentials on response topics. */
static char key_cert_topic_get[75] = "certificate/" CONFIG_AWS_IOT_CLIENT_ID_STATIC "/create";

/* Topic types used for filtering. */
enum topic_type {
	KEY_TOPIC,
	CERT_TOPIC,
	UNKNOWN
};

/* Intermediate credential buffers. */

/* Client certificate */
static char client_certificate[2048];
static size_t client_certificate_len;

/* Private key */
static char private_key[2048];
static size_t private_key_len;

static void connect_after_provisioning_work_fn(struct k_work *work)
{
	struct aws_iot_config config = {
		.sec_tag = (int)sec_tag
	};

	/* Clear custom subscriptions to avoid subscribing to the provisioning topics
	 * in the new connection.
	 */
	aws_iot_subscription_topics_clear();

	int err = aws_iot_connect(&config);
	if (err) {
		printk("aws_iot_connect, error: %d\n", err);
	}
}

static enum topic_type topic_filter(const char *topic, size_t topic_len)
{
	if (strstr(topic, KEY_TOPIC_FILTER) != NULL) {
		return KEY_TOPIC;
	} else if (strstr(topic, CERT_TOPIC_FILTER) != NULL) {
		return CERT_TOPIC;
	}

	return UNKNOWN;
}

static void incoming_data_handle(const char *buf,
				 size_t buf_len,
				 const char *topic,
				 size_t topic_len)
{
	int err;

	LOG_INF("%d bytes received from AWS IoT console: Topic: %.*s:", buf_len, topic_len, topic);
	LOG_INF("\n\n%.*s", buf_len, buf);

	if (device_provisioned) {
		LOG_WRN("device already provisioned");
		return;
	}

	enum topic_type type = topic_filter(topic, topic_len);

	switch (type) {
	case KEY_TOPIC:
		LOG_INF("KEY_TOPIC");
		__ASSERT_NO_MSG(sizeof(private_key) > buf_len);
		memcpy(&private_key, buf, buf_len);
		private_key_len = buf_len;
		LOG_INF("Private key copied");
		break;
	case CERT_TOPIC:
		LOG_INF("CERT_TOPIC");
		__ASSERT_NO_MSG(sizeof(client_certificate) > buf_len);
		memcpy(&client_certificate, buf, buf_len);
		client_certificate_len = buf_len;
		LOG_INF("Certificate copied");
		break;
	default:
		LOG_ERR("Unknown incoming topic!");
		return;
	}

	if (client_certificate_len == 0 || private_key_len == 0) {
		LOG_INF("Not all credentials has been received, abort provisioning");
		return;
	}

	LOG_INF("Provision credentials");

	/* Disconnect client before shutting down the modem. */
	(void)aws_iot_disconnect();

	LOG_INF("AWS IoT Client disconnected");

	/* Go offline in order to provision credentials. */
	lte_lc_offline();

	LOG_INF("Modem set in offline mode");

	/* Write Private key */
	err = modem_key_mgmt_write(sec_tag,
				   MODEM_KEY_MGMT_CRED_TYPE_PRIVATE_CERT,
				   &private_key,
				   private_key_len);
	if (err) {
		LOG_ERR("Failed writing private key to the modem");
	}

	/* Write client certificate */
	err = modem_key_mgmt_write(sec_tag,
				   MODEM_KEY_MGMT_CRED_TYPE_PUBLIC_CERT,
				   &client_certificate,
				   client_certificate_len);
	if (err) {
		LOG_ERR("Failed writing client certificate to the modem");
	}

	/* Write CA certificate */
	err = modem_key_mgmt_write(sec_tag,
				   MODEM_KEY_MGMT_CRED_TYPE_CA_CHAIN,
				   CA,
				   sizeof(CA));
	if (err) {
		LOG_ERR("Failed writing client certificate to the modem");
	}

	LOG_INF("Credentials written to the modem!");

	/* Connect to LTE.*/
	lte_lc_connect();

	/* Schedule a new connection. */
	k_work_schedule(&connect_after_provisioning_work, K_SECONDS(1));

	device_provisioned = true;
}

static void credentials_get()
{
	/* Publish blank message to certificate/${deviceId}/create to get credentials. */
	struct aws_iot_data tx_data = {
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
		.topic.type = AWS_IOT_SHADOW_TOPIC_NONE,
		.topic.str = key_cert_topic_get,
		.topic.len = strlen(key_cert_topic_get),
		.ptr = "",
		.len = strlen("")
	};

	LOG_INF("Publishing blanc message to AWS IoT broker");

	int err = aws_iot_send(&tx_data);
	if (err) {
		LOG_INF("aws_iot_send, error: %d", err);
	}
}

void aws_iot_event_handler(const struct aws_iot_evt *const evt)
{
	switch (evt->type) {
	case AWS_IOT_EVT_CONNECTING:
		LOG_INF("AWS_IOT_EVT_CONNECTING");
		break;
	case AWS_IOT_EVT_CONNECTED:
		LOG_INF("AWS_IOT_EVT_CONNECTED");
		break;
	case AWS_IOT_EVT_READY:
		LOG_INF("AWS_IOT_EVT_READY");
		k_sem_give(&cloud_connected);
		break;
	case AWS_IOT_EVT_DISCONNECTED:
		LOG_INF("AWS_IOT_EVT_DISCONNECTED");
		break;
	case AWS_IOT_EVT_DATA_RECEIVED:
		LOG_INF("AWS_IOT_EVT_DATA_RECEIVED");
		incoming_data_handle(evt->data.msg.ptr,
				     evt->data.msg.len,
				     evt->data.msg.topic.str,
				     evt->data.msg.topic.len);
		break;
	case AWS_IOT_EVT_PUBACK:
		LOG_INF("AWS_IOT_EVT_PUBACK, message ID: %d", evt->data.message_id);
		break;
	case AWS_IOT_EVT_ERROR:
		LOG_INF("AWS_IOT_EVT_ERROR, %d", evt->data.err);
		break;
	default:
		LOG_INF("Unknown AWS IoT event type: %d", evt->type);
		break;
	}
}

static int app_topics_subscribe(void)
{
	int err;
	const struct aws_iot_topic_data topic_list[] = {
		{ AWS_IOT_SHADOW_TOPIC_NONE, key_cert_topic, strlen(key_cert_topic) }
	};

	err = aws_iot_subscription_topics_add(topic_list, ARRAY_SIZE(topic_list));
	if (err) {
		LOG_INF("aws_iot_subscription_topics_add, error: %d", err);
	}

	return err;
}

void main(void)
{
	int err;

	LOG_INF("AWS IoT provisioning sample started");

	k_work_init_delayable(&connect_after_provisioning_work, connect_after_provisioning_work_fn);

	/* Initialize AWS IoT library. */
	err = aws_iot_init(NULL, aws_iot_event_handler);
	if (err) {
		LOG_ERR("AWS IoT library could not be initialized, error: %d", err);
	}

	/** Add subscription to application specific topic. */
	err = app_topics_subscribe();
	if (err) {
		LOG_ERR("Adding application specific topics failed, error: %d", err);
	}

	/* Connect to AWS IoT. */
	err = aws_iot_connect(NULL);
	if (err) {
		LOG_ERR("aws_iot_connect, error: %d", err);
	}

	/* Wait for the connection to AWS IoT to be established. */
	k_sem_take(&cloud_connected, K_FOREVER);

	/* Request credentials */
	credentials_get();
}
