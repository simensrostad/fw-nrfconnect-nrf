/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * Generated using zcbor version 0.5.1
 * https://github.com/NordicSemiconductor/zcbor
 * Generated with a --default-max-qty of 10
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef MSG_ENCODE_H__
#define MSG_ENCODE_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "zcbor_encode.h"
#include "msg_encode_types.h"

#if DEFAULT_MAX_QTY != 10
#error "The type file was generated with a different default_max_qty than this file"
#endif

int cbor_encode_message_out(uint8_t *payload, size_t payload_len, const struct message_out *input,
			    size_t *payload_len_out);

#endif /* MSG_ENCODE_H__ */
