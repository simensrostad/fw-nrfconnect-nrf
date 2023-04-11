/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/zbus/zbus.h>

/* JSON */
#include <zephyr/data/json.h>

/* Protobuf */
#include <pb_encode.h>
#include <pb_decode.h>

/* Local */
#include "src/modules/encoder/protobuf/simple.pb.h"
#include "message_channel.h"

/* Register log module */
LOG_MODULE_REGISTER(encoder, CONFIG_MQTT_SAMPLE_ENCODER_LOG_LEVEL);

static int json_encode(struct payload *payload)
{
	int err;
	const struct json_obj_descr root_object_description[] = {
		JSON_OBJ_DESCR_PRIM(struct raw, id, JSON_TOK_NUMBER),
		JSON_OBJ_DESCR_PRIM(struct raw, type, JSON_TOK_STRING),
		JSON_OBJ_DESCR_PRIM(struct raw, name, JSON_TOK_STRING),
		JSON_OBJ_DESCR_PRIM(struct raw, uptime, JSON_TOK_NUMBER),
	};

	err = json_obj_encode_buf(root_object_description,
				  ARRAY_SIZE(root_object_description),
				  &payload->raw,
				  payload->encoded.buffer,
				  sizeof(payload->encoded.buffer));
	if (err) {
		LOG_ERR("json_obj_encode_buf, error: %d", err);
		return -EFAULT;
	}

	payload->encoded.length = strlen(payload->encoded.buffer);
	payload->encoded.format = JSON;
	return 0;
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

static int encode(struct payload *payload)
{
	int err = 0;

	if (IS_ENABLED(CONFIG_MQTT_SAMPLE_ENCODER_FORMAT_JSON)) {
		err = json_encode(payload);
	} else if (IS_ENABLED(CONFIG_MQTT_SAMPLE_ENCODER_FORMAT_PROTOBUF)) {
		err = protobuf_encode(payload);
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

		zbus_chan_claim(&PAYLOAD_CHAN, K_FOREVER);

		/* Get payload buffer from channel. */
		payload = zbus_chan_msg(chan);

		err = encode(payload);
		if (err) {
			LOG_ERR("encode, error: %d", err);
			SEND_FATAL_ERROR();
			return;
		}

		zbus_chan_finish(&PAYLOAD_CHAN);
	}
}

/* Register listener - encoder_callback will be called everytime a channel that the module listens
 * on receives a new message.
 */
ZBUS_LISTENER_DEFINE(encoder, encoder_callback);
