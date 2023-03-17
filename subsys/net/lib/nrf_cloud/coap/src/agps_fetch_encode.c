/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * Generated using zcbor version 0.6.0
 * https://github.com/NordicSemiconductor/zcbor
 * Generated with a --default-max-qty of 10
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "zcbor_encode.h"
#include "agps_fetch_encode.h"

#if DEFAULT_MAX_QTY != 10
#error "The type file was generated with a different default_max_qty than this file"
#endif

static bool encode_customTypes(zcbor_state_t *state, const struct customTypes *input);
static bool encode_eci(zcbor_state_t *state, const struct eci *input);
static bool encode_filtered(zcbor_state_t *state, const struct filtered *input);
static bool encode_mask(zcbor_state_t *state, const struct mask *input);
static bool encode_mcc(zcbor_state_t *state, const struct mcc *input);
static bool encode_mnc(zcbor_state_t *state, const struct mnc *input);
static bool encode_requestType(zcbor_state_t *state, const struct requestType *input);
static bool encode_rsrp(zcbor_state_t *state, const struct rsrp *input);
static bool encode_tac(zcbor_state_t *state, const struct tac *input);
static bool encode_agps_type(zcbor_state_t *state, const struct agps_type_ *input);
static bool encode_agps(zcbor_state_t *state, const struct agps *input);

static bool encode_customTypes(zcbor_state_t *state, const struct customTypes *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result =
		(((((zcbor_uint32_put(state, (0)))) &&
		   (((((*input)._customTypes_val >= 1) && ((*input)._customTypes_val <= 10)) ||
		     (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)) &&
		    (zcbor_uint32_encode(state, (&(*input)._customTypes_val)))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_eci(zcbor_state_t *state, const struct eci *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((((zcbor_uint32_put(state, (1)))) &&
			     (((((*input)._eci_val >= 0) && ((*input)._eci_val <= 268435455)) ||
			       (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)) &&
			      (zcbor_uint32_encode(state, (&(*input)._eci_val)))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_filtered(zcbor_state_t *state, const struct filtered *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((((zcbor_uint32_put(state, (2)))) &&
			     ((zcbor_bool_encode(state, (&(*input)._filtered_val)))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_mask(zcbor_state_t *state, const struct mask *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((((zcbor_uint32_put(state, (3)))) &&
			     (((((*input)._mask_val >= 0) && ((*input)._mask_val <= 90)) ||
			       (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)) &&
			      (zcbor_uint32_encode(state, (&(*input)._mask_val)))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_mcc(zcbor_state_t *state, const struct mcc *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((((zcbor_uint32_put(state, (4)))) &&
			     ((zcbor_uint32_encode(state, (&(*input)._mcc_val)))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_mnc(zcbor_state_t *state, const struct mnc *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result =
		(((((zcbor_uint32_put(state, (5)))) &&
		   ((((*input)._mnc_val_choice == _mnc_val_tstr) ?
			     ((zcbor_tstr_encode(state, (&(*input)._mnc_val_tstr)))) :
			     (((*input)._mnc_val_choice == _mnc_val_int) ?
				      ((zcbor_int32_encode(state, (&(*input)._mnc_val_int)))) :
				      false))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_requestType(zcbor_state_t *state, const struct requestType *input)
{
	zcbor_print("%s\r\n", __func__);
	struct zcbor_string tmp_str;

	bool tmp_result = (((
		((zcbor_uint32_put(state, (6)))) &&
		((((*input)._requestType_val_choice == _requestType_val_rtLocation_tstr) ?
			  ((zcbor_tstr_encode(state, ((tmp_str.value = (uint8_t *)"rtLocation",
						       tmp_str.len = sizeof("rtLocation") - 1,
						       &tmp_str))))) :
			 (((*input)._requestType_val_choice == _requestType_val_rtAssistance_tstr) ?
				   ((zcbor_tstr_encode(state,
						       ((tmp_str.value = (uint8_t *)"rtAssistance",
							 tmp_str.len = sizeof("rtAssistance") - 1,
							 &tmp_str))))) :
				   (((*input)._requestType_val_choice ==
				     _requestType_val_custom_tstr) ?
					    ((zcbor_tstr_encode(
						    state, ((tmp_str.value = (uint8_t *)"custom",
							     tmp_str.len = sizeof("custom") - 1,
							     &tmp_str))))) :
					    false)))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_rsrp(zcbor_state_t *state, const struct rsrp *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((((zcbor_uint32_put(state, (7)))) &&
			     (((((*input)._rsrp_val >= -157) && ((*input)._rsrp_val <= -44)) ||
			       (zcbor_error(state, ZCBOR_ERR_WRONG_RANGE), false)) &&
			      (zcbor_int32_encode(state, (&(*input)._rsrp_val)))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_tac(zcbor_state_t *state, const struct tac *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((((zcbor_uint32_put(state, (8)))) &&
			     ((zcbor_uint32_encode(state, (&(*input)._tac_val)))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_agps_type(zcbor_state_t *state, const struct agps_type_ *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result = (((
		((*input)._agps_type_choice == _agps_type__customTypes) ?
			((encode_customTypes(state, (&(*input)._agps_type__customTypes)))) :
			(((*input)._agps_type_choice == _agps_type__eci) ?
				 ((encode_eci(state, (&(*input)._agps_type__eci)))) :
				 (((*input)._agps_type_choice == _agps_type__filtered) ?
					  ((encode_filtered(state,
							    (&(*input)._agps_type__filtered)))) :
					  (((*input)._agps_type_choice == _agps_type__mask) ?
						  ((encode_mask(state,
								 (&(*input)._agps_type__mask)))) :
						  (((*input)._agps_type_choice == _agps_type__mcc) ?
							    ((encode_mcc(
								    state,
								    (&(*input)._agps_type__mcc)))) :
							    (((*input)._agps_type_choice ==
							      _agps_type__mnc) ?
								((encode_mnc(
								  state,
								  (&(*input)._agps_type__mnc)))) :
								  (((*input)._agps_type_choice ==
								    _agps_type__requestType) ?
								((encode_requestType(
							    state,
							    (&(*input)._agps_type__requestType)))) :
							    (((*input)._agps_type_choice ==
								_agps_type__rsrp) ?
								((encode_rsrp(
								   state,
								   (&(*input)._agps_type__rsrp)))) :
								(((*input)._agps_type_choice ==
								 _agps_type__tac) ?
									((encode_tac(
										state,
								(&(*input)._agps_type__tac)))) :
										false)))))))))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

static bool encode_agps(zcbor_state_t *state, const struct agps *input)
{
	zcbor_print("%s\r\n", __func__);

	bool tmp_result =
		(((zcbor_list_start_encode(state, 20) &&
		   ((zcbor_multi_encode_minmax(
			    0, 10, &(*input).__agps_type_count, (zcbor_encoder_t *)encode_agps_type,
			    state, (&(*input).__agps_type), sizeof(struct agps_type_))) ||
		    (zcbor_list_map_end_force_encode(state), false)) &&
		   zcbor_list_end_encode(state, 20))));

	if (!tmp_result)
		zcbor_trace();

	return tmp_result;
}

int cbor_encode_agps(uint8_t *payload, size_t payload_len, const struct agps *input,
		     size_t *payload_len_out)
{
	zcbor_state_t states[5];

	zcbor_new_state(states, sizeof(states) / sizeof(zcbor_state_t), payload, payload_len, 1);

	bool ret = encode_agps(states, input);

	if (ret && (payload_len_out != NULL)) {
		*payload_len_out = MIN(payload_len, (size_t)states[0].payload - (size_t)payload);
	}

	if (!ret) {
		int err = zcbor_pop_error(states);

		zcbor_print("Return error: %d\r\n", err);
		return (err == ZCBOR_SUCCESS) ? ZCBOR_ERR_UNKNOWN : err;
	}
	return ZCBOR_SUCCESS;
}
