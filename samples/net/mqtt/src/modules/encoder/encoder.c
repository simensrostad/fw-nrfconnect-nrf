/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

/* JSON */
#include <cJSON.h>

/* Protobuf */
#include <pb_encode.h>
#include <pb_decode.h>

/* CBOR */
#include <zcbor_decode.h>
#include <zcbor_encode.h>
#include <zcbor_common.h>

/* Local */
#include "src/modules/encoder/protobuf/simple.pb.h"
#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(encoder, CONFIG_MQTT_SAMPLE_ENCODER_LOG_LEVEL);

static int json_encode(struct payload *payload)
{
	int err = 0;
	cJSON *root;
	cJSON *number;
	cJSON *string;

	root = cJSON_CreateObject();
	if (root == NULL) {
		return -ENOMEM;
	}

	/*  ID */
	number = cJSON_CreateNumber(payload->raw.id);
	if (number == NULL) {
		goto exit;
	}

	if (!cJSON_AddItemToObject(root, "id", number)) {
		cJSON_Delete(number);
		goto exit;
	}

	/* Uptime */
	number = cJSON_CreateNumber(payload->raw.uptime);
	if (number == NULL) {
		goto exit;
	}

	if (!cJSON_AddItemToObject(root, "uptime", number)) {
		cJSON_Delete(number);
		goto exit;
	}

	/* Type */
	string = cJSON_CreateString(payload->raw.type);
	if (string == NULL) {
		goto exit;
	}

	if (!cJSON_AddItemToObject(root, "type", string)) {
		cJSON_Delete(string);
		goto exit;
	}

	/* Name */
	string = cJSON_CreateString(payload->raw.name);
	if (string == NULL) {
		goto exit;
	}

	if (!cJSON_AddItemToObject(root, "name", string)) {
		cJSON_Delete(string);
		goto exit;
	}

	memcpy(payload->encoded.buffer, cJSON_PrintUnformatted(root), sizeof(payload->encoded.buffer));

	payload->encoded.format = JSON;
	payload->encoded.length = strlen(payload->encoded.buffer);

exit:
	cJSON_Delete(root);
	return err;
}

static int protobuf_encode(struct payload *payload)
{
	bool encode_status;
	Message message = {
		.id = payload->raw.id,
		.uptime = payload->raw.uptime
	};


	strcpy(message.type, payload->raw.type);
	strcpy(message.name, payload->raw.name);

	/* Create a stream that will write to our buffer. */
	pb_ostream_t stream = pb_ostream_from_buffer(payload->encoded.buffer,
						     sizeof(payload->encoded.buffer));

	encode_status = pb_encode(&stream, Message_fields, &message);
	if (!encode_status) {
		LOG_ERR("pb_encode, error: %s", PB_GET_ERROR(&stream));
		return -EFAULT;
	}

	payload->encoded.length = stream.bytes_written;
	payload->encoded.format = PROTOBUF;
	return 0;
}

static int cbor_encode(struct payload *payload)
{
	bool encode_status;

	/* Create zcbor state variable for encoding. */
	ZCBOR_STATE_E(encoding_state, 0, payload->encoded.buffer, sizeof(payload->encoded.buffer), 0);

	/* Get pointer to encoding state buffer. Will be used to determine the total length of the
	 * encoded data.
	 */
	const uint8_t *first = encoding_state->payload;

	/* Encode a text string into the cbor_payload buffer */
	encode_status = zcbor_uint32_put(encoding_state, payload->raw.id);
	if (!encode_status) {
		LOG_ERR("zcbor_tstr_put_lit, error: %d", zcbor_peek_error(encoding_state));
		return -EFAULT;
	}

	encode_status = zcbor_tstr_put_lit(encoding_state, payload->raw.type);
	if (!encode_status) {
		LOG_ERR("zcbor_tstr_put_lit, error: %d", zcbor_peek_error(encoding_state));
		return -EFAULT;
	}

	encode_status = zcbor_tstr_put_lit(encoding_state, payload->raw.name);
	if (!encode_status) {
		LOG_ERR("zcbor_tstr_put_lit, error: %d", zcbor_peek_error(encoding_state));
		return -EFAULT;
	}

	encode_status = zcbor_uint32_put(encoding_state, payload->raw.uptime);
	if (!encode_status) {
		LOG_ERR("zcbor_tstr_put_lit, error: %d", zcbor_peek_error(encoding_state));
		return -EFAULT;
	}

	/* Calculate the total length of the encoded data. */
	payload->encoded.length = encoding_state->payload - first;
	payload->encoded.format = CBOR;
	return 0;
}

static int encode(struct payload *payload)
{
	int err = 0;

	if (IS_ENABLED(CONFIG_MQTT_SAMPLE_ENCODER_FORMAT_JSON)) {

		err = json_encode(payload);

	} else if (IS_ENABLED(CONFIG_MQTT_SAMPLE_ENCODER_FORMAT_PROTOBUF)) {

		err = protobuf_encode(payload);

	} else if (IS_ENABLED(CONFIG_MQTT_SAMPLE_ENCODER_FORMAT_CBOR)) {

		err = cbor_encode(payload);

	} else {
		__ASSERT(false, "Unknown encoding");
	}

	return err;
}

void encoder_callback(const struct zbus_channel *chan)
{
	int err;
	struct payload *payload;

	/* Need to claim the channel before changing the payload, this is important. */

	if (&PAYLOAD_CHAN == chan) {

		/* Get payload buffer from channel. */
		payload = zbus_chan_msg(chan);

		err = encode(payload);
		if (err) {
			LOG_ERR("encode, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}
	}
}

/* Register listener - encoder_callback will be called everytime a channel that the module listens
 * on receives a new message.
 */
ZBUS_LISTENER_DEFINE(encoder, encoder_callback);
