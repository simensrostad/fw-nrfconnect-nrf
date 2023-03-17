/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * Generated using zcbor version 0.5.1
 * https://github.com/NordicSemiconductor/zcbor
 * Generated with a --default-max-qty of 10
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef GROUND_FIX_ENCODE_TYPES_H__
#define GROUND_FIX_ENCODE_TYPES_H__

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

struct cell_earfcn {
	uint32_t _cell_earfcn;
};

struct cell_adv {
	uint32_t _cell_adv;
};

struct ncell_rsrp {
	int32_t _ncell_rsrp;
};

struct ncell_rsrq_ {
	union {
		float _ncell_rsrq_float32;
		int32_t _ncell_rsrq_int;
	};
	enum {
		_ncell_rsrq_float32,
		_ncell_rsrq_int,
	} _ncell_rsrq_choice;
};

struct ncell_timeDiff {
	int32_t _ncell_timeDiff;
};

struct ncell {
	uint32_t _ncell_earfcn;
	uint32_t _ncell_pci;
	struct ncell_rsrp _ncell_rsrp;
	uint_fast32_t _ncell_rsrp_present;
	struct ncell_rsrq_ _ncell_rsrq;
	uint_fast32_t _ncell_rsrq_present;
	struct ncell_timeDiff _ncell_timeDiff;
	uint_fast32_t _ncell_timeDiff_present;
};

struct cell_nmr_ {
	struct ncell _cell_nmr_ncells[5];
	uint_fast32_t _cell_nmr_ncells_count;
};

struct cell_rsrp {
	int32_t _cell_rsrp;
};

struct cell_rsrq_ {
	union {
		float _cell_rsrq_float32;
		int32_t _cell_rsrq_int;
	};
	enum {
		_cell_rsrq_float32,
		_cell_rsrq_int,
	} _cell_rsrq_choice;
};

struct cell {
	uint32_t _cell_mcc;
	uint32_t _cell_mnc;
	uint32_t _cell_eci;
	uint32_t _cell_tac;
	struct cell_earfcn _cell_earfcn;
	uint_fast32_t _cell_earfcn_present;
	struct cell_adv _cell_adv;
	uint_fast32_t _cell_adv_present;
	struct cell_nmr_ _cell_nmr;
	uint_fast32_t _cell_nmr_present;
	struct cell_rsrp _cell_rsrp;
	uint_fast32_t _cell_rsrp_present;
	struct cell_rsrq_ _cell_rsrq;
	uint_fast32_t _cell_rsrq_present;
};

struct lte_ar {
	struct cell _lte_ar__cell[5];
	uint_fast32_t _lte_ar__cell_count;
};

struct ground_fix_req_lte {
	struct lte_ar _ground_fix_req_lte;
};

struct ap_age {
	uint32_t _ap_age;
};

struct ap_signalStrength {
	int32_t _ap_signalStrength;
};

struct ap_channel {
	uint32_t _ap_channel;
};

struct ap_frequency {
	uint32_t _ap_frequency;
};

struct ap_ssid {
	struct zcbor_string _ap_ssid;
};

struct ap {
	struct zcbor_string _ap_macAddress;
	struct ap_age _ap_age;
	uint_fast32_t _ap_age_present;
	struct ap_signalStrength _ap_signalStrength;
	uint_fast32_t _ap_signalStrength_present;
	struct ap_channel _ap_channel;
	uint_fast32_t _ap_channel_present;
	struct ap_frequency _ap_frequency;
	uint_fast32_t _ap_frequency_present;
	struct ap_ssid _ap_ssid;
	uint_fast32_t _ap_ssid_present;
};

struct wifi_ob {
	struct ap _wifi_ob_accessPoints__ap[20];
	uint_fast32_t _wifi_ob_accessPoints__ap_count;
};

struct ground_fix_req_wifi {
	struct wifi_ob _ground_fix_req_wifi;
};

struct ground_fix_req {
	struct ground_fix_req_lte _ground_fix_req_lte;
	uint_fast32_t _ground_fix_req_lte_present;
	struct ground_fix_req_wifi _ground_fix_req_wifi;
	uint_fast32_t _ground_fix_req_wifi_present;
};

#endif /* GROUND_FIX_ENCODE_TYPES_H__ */
