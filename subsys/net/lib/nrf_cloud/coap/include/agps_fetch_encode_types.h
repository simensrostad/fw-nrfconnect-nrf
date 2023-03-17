/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * Generated using zcbor version 0.6.0
 * https://github.com/NordicSemiconductor/zcbor
 * Generated with a --default-max-qty of 10
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef AGPS_FETCH_ENCODE_TYPES_H__
#define AGPS_FETCH_ENCODE_TYPES_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>
#include "zcbor_encode.h"

/** Which value for --default-max-qty this file was created with.
 *
 *  The define is used in the other generated file to do a build-time
 *  compatibility check.
 *
 *  See `zcbor --help` for more information about --default-max-qty
 */
#define DEFAULT_MAX_QTY 10

struct customTypes {
	uint32_t _customTypes_val;
};

struct eci {
	uint32_t _eci_val;
};

struct filtered {
	bool _filtered_val;
};

struct mask {
	uint32_t _mask_val;
};

struct mcc {
	uint32_t _mcc_val;
};

struct mnc {
	union {
		struct zcbor_string _mnc_val_tstr;
		int32_t _mnc_val_int;
	};
	enum {
		_mnc_val_tstr,
		_mnc_val_int,
	} _mnc_val_choice;
};

struct requestType {
	enum {
		_requestType_val_rtLocation_tstr,
		_requestType_val_rtAssistance_tstr,
		_requestType_val_custom_tstr,
	} _requestType_val_choice;
};

struct rsrp {
	int32_t _rsrp_val;
};

struct tac {
	uint32_t _tac_val;
};

struct agps_type_ {
	union {
		struct customTypes _agps_type__customTypes;
		struct eci _agps_type__eci;
		struct filtered _agps_type__filtered;
		struct mask _agps_type__mask;
		struct mcc _agps_type__mcc;
		struct mnc _agps_type__mnc;
		struct requestType _agps_type__requestType;
		struct rsrp _agps_type__rsrp;
		struct tac _agps_type__tac;
	};
	enum {
		_agps_type__customTypes,
		_agps_type__eci,
		_agps_type__filtered,
		_agps_type__mask,
		_agps_type__mcc,
		_agps_type__mnc,
		_agps_type__requestType,
		_agps_type__rsrp,
		_agps_type__tac,
	} _agps_type_choice;
};

struct agps {
	struct agps_type_ __agps_type[10];
	uint_fast32_t __agps_type_count;
};

#endif /* AGPS_FETCH_ENCODE_TYPES_H__ */
