/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include <zephyr/kernel.h>
#include <zephyr/net/coap.h>
#include <modem/lte_lc.h>
#include <net/wifi_location_common.h>
#include <net/nrf_cloud_codec.h>
#include <cJSON.h>
#include "nrf_cloud_codec_internal.h"
#include "ground_fix_encode_types.h"
#include "ground_fix_encode.h"
#include "ground_fix_decode_types.h"
#include "ground_fix_decode.h"
#include "agps_encode_types.h"
#include "agps_encode.h"
#include "pgps_encode_types.h"
#include "pgps_encode.h"
#include "pgps_decode_types.h"
#include "pgps_decode.h"
#include "msg_encode_types.h"
#include "msg_encode.h"
#include "coap_codec.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(coap_codec, CONFIG_NRF_CLOUD_COAP_LOG_LEVEL);

static int encode_message(const char *app_id, const char *str,
			  const struct nrf_cloud_gnss_pvt *pvt,
			  double float_val, int int_val,
			  int64_t ts, uint8_t *buf, size_t *len,
			  enum coap_content_format fmt)
{
	int err;

	if (fmt == COAP_CONTENT_FORMAT_APP_CBOR) {
		struct message_out input;
		size_t out_len;

		memset(&input, 0, sizeof(struct message_out));
		input._message_out_appId.value = app_id;
		input._message_out_appId.len = strlen(app_id);
		if (str) {
			input._message_out_data_choice = _message_out_data_tstr;
			input._message_out_data_tstr.value = str;
			input._message_out_data_tstr.len = strlen(str);
		} else if (pvt) {
			input._message_out_data_choice = _message_out_data__pvt;
			input._message_out_data__pvt._pvt_lat = pvt->lat;
			input._message_out_data__pvt._pvt_lng = pvt->lon;
			input._message_out_data__pvt._pvt_acc = pvt->accuracy;
			if (pvt->has_speed) {
				input._message_out_data__pvt._pvt_spd._pvt_spd = pvt->speed;
				input._message_out_data__pvt._pvt_spd_present = true;
			}
			if (pvt->has_heading) {
				input._message_out_data__pvt._pvt_hdg._pvt_hdg = pvt->heading;
				input._message_out_data__pvt._pvt_hdg_present = true;
			}
			if (pvt->has_alt) {
				input._message_out_data__pvt._pvt_alt._pvt_alt = pvt->alt;
				input._message_out_data__pvt._pvt_alt_present = true;
			}
		} else if (!isnan(float_val)) {
			input._message_out_data_choice = _message_out_data_float;
			input._message_out_data_float = float_val;
		} else {
			input._message_out_data_choice = _message_out_data_int;
			input._message_out_data_int = int_val;
		}
		input._message_out_ts._message_out_ts = ts;
		input._message_out_ts_present = true;
		err = cbor_encode_message_out(buf, *len, &input, &out_len);
		if (err) {
			LOG_ERR("Error %d encoding message", err);
			err = -EINVAL;
			*len = 0;
		} else {
			*len = out_len;
		}
	} else {
		struct nrf_cloud_data out;

		err = nrf_cloud_encode_message(app_id, float_val, str, NULL, ts, &out);
		if (err) {
			*len = 0;
		} else if (*len < out.len) {
			*len = 0;
			cJSON_free((void *)out.ptr);
			err = -E2BIG;
		} else {
			*len = out.len;
			memcpy(buf, out.ptr, *len);
			buf[*len] = '\0';
			cJSON_free((void *)out.ptr);
		}
	}
	return err;
}

int coap_codec_message_encode(const char *app_id,
			      const char *str_val, double float_val, int int_val,
			      int64_t ts, uint8_t *buf, size_t *len, enum coap_content_format fmt)
{
	__ASSERT_NO_MSG(app_id != NULL);
	__ASSERT_NO_MSG(buf != NULL);
	__ASSERT_NO_MSG(len != NULL);

	return encode_message(app_id, str_val, NULL, float_val, int_val, ts, buf, len, fmt);
}

int coap_codec_sensor_encode(const char *app_id, double float_val,
			     int64_t ts, uint8_t *buf, size_t *len, enum coap_content_format fmt)
{
	__ASSERT_NO_MSG(app_id != NULL);
	__ASSERT_NO_MSG(buf != NULL);
	__ASSERT_NO_MSG(len != NULL);

	return encode_message(app_id, NULL, NULL, float_val, 0, ts, buf, len, fmt);
}

int coap_codec_pvt_encode(const char *app_id, const struct nrf_cloud_gnss_pvt *pvt,
			  int64_t ts, uint8_t *buf, size_t *len, enum coap_content_format fmt)
{
	__ASSERT_NO_MSG(app_id != NULL);
	__ASSERT_NO_MSG(pvt != NULL);
	__ASSERT_NO_MSG(buf != NULL);
	__ASSERT_NO_MSG(len != NULL);

	return encode_message(app_id, NULL, pvt, 0, 0, ts, buf, len, fmt);
}

static void copy_cell(struct cell *dst, struct lte_lc_cell const *const src)
{
	dst->_cell_mcc = src->mcc;
	dst->_cell_mnc = src->mnc;
	dst->_cell_eci = src->id;
	dst->_cell_tac = src->tac;
	if (src->earfcn != NRF_CLOUD_LOCATION_CELL_OMIT_EARFCN) {
		dst->_cell_earfcn._cell_earfcn = src->earfcn;
		dst->_cell_earfcn_present = true;
	} else {
		dst->_cell_earfcn_present = false;
	}
	if (src->timing_advance != NRF_CLOUD_LOCATION_CELL_OMIT_TIME_ADV) {
		uint16_t t_adv = src->timing_advance;

		if (t_adv > NRF_CLOUD_LOCATION_CELL_TIME_ADV_MAX) {
			t_adv = NRF_CLOUD_LOCATION_CELL_TIME_ADV_MAX;
		}
		dst->_cell_adv._cell_adv = t_adv;
		dst->_cell_adv_present = true;
	} else {
		dst->_cell_adv_present = false;
	}
	if (src->rsrp != NRF_CLOUD_LOCATION_CELL_OMIT_RSRP) {
		dst->_cell_rsrp._cell_rsrp = RSRP_IDX_TO_DBM(src->rsrp);
		dst->_cell_rsrp_present = true;
	} else {
		dst->_cell_rsrp_present = false;
	}
	if (src->rsrq != NRF_CLOUD_LOCATION_CELL_OMIT_RSRQ) {
		dst->_cell_rsrq._cell_rsrq_float32 = RSRQ_IDX_TO_DB(src->rsrq);
		dst->_cell_rsrq._cell_rsrq_choice = _cell_rsrq_float32;
		dst->_cell_rsrq_present = true;
	} else {
		dst->_cell_rsrq_present = false;
	}
}

static void copy_ncells(struct ncell *dst, int num, struct lte_lc_ncell *src)
{
	for (int i = 0; i < num; i++) {
		dst->_ncell_earfcn = src->earfcn;
		dst->_ncell_pci = src->phys_cell_id;
		if (src->rsrp != NRF_CLOUD_LOCATION_CELL_OMIT_RSRP) {
			dst->_ncell_rsrp._ncell_rsrp = RSRP_IDX_TO_DBM(src->rsrp);
			dst->_ncell_rsrp_present = true;
		} else {
			dst->_ncell_rsrp_present = false;
		}
		if (src->rsrq != NRF_CLOUD_LOCATION_CELL_OMIT_RSRQ) {
			dst->_ncell_rsrq._ncell_rsrq_float32 = RSRQ_IDX_TO_DB(src->rsrq);
			dst->_ncell_rsrq._ncell_rsrq_choice = _ncell_rsrq_float32;
			dst->_ncell_rsrq_present = true;
		} else {
			dst->_ncell_rsrq_present = false;
		}
		if (src->time_diff != LTE_LC_CELL_TIME_DIFF_INVALID) {
			dst->_ncell_timeDiff._ncell_timeDiff = src->time_diff;
			dst->_ncell_timeDiff_present = true;
		} else {
			dst->_ncell_timeDiff_present = false;
		}
		src++;
		dst++;
	}
}

static void copy_cell_info(struct lte_ar *lte_encode,
			   struct lte_lc_cells_info const *const cell_info)
{
	if (cell_info != NULL) {
		struct cell *cell = lte_encode->_lte_ar__cell;
		size_t num_cells = ARRAY_SIZE(lte_encode->_lte_ar__cell);

		if (cell_info->current_cell.id != LTE_LC_CELL_EUTRAN_ID_INVALID) {
			lte_encode->_lte_ar__cell_count++;
			copy_cell(cell, &cell_info->current_cell);
			cell->_cell_nmr._cell_nmr_ncells_count = cell_info->ncells_count;
			if (cell_info->ncells_count) {
				copy_ncells(cell->_cell_nmr._cell_nmr_ncells,
					    MIN(ARRAY_SIZE(cell->_cell_nmr._cell_nmr_ncells),
						cell_info->ncells_count),
					    cell_info->neighbor_cells);
			}
			cell++;
			num_cells--;
		}
		if (cell_info->gci_cells != NULL) {
			for (int i = 0; i < cell_info->gci_cells_count; i++) {
				lte_encode->_lte_ar__cell_count++;
				copy_cell(cell, &cell_info->gci_cells[i]);
				cell++;
				num_cells--;
				if (!num_cells) {
					break;
				}
			}
		}
	}
}

static void copy_wifi_info(struct wifi_ob *wifi_encode,
			   struct wifi_scan_info const *const wifi_info)
{
	struct ap *dst = wifi_encode->_wifi_ob_accessPoints__ap;
	struct wifi_scan_result *src = wifi_info->ap_info;
	size_t num_aps = ARRAY_SIZE(wifi_encode->_wifi_ob_accessPoints__ap);

	wifi_encode->_wifi_ob_accessPoints__ap_count = 0;

	for (int i = 0; i < wifi_info->cnt; i++) {
		wifi_encode->_wifi_ob_accessPoints__ap_count++;

		dst->_ap_macAddress.value = src->mac;
		dst->_ap_macAddress.len = src->mac_length;
		dst->_ap_age_present = false;
		if (src->rssi != NRF_CLOUD_LOCATION_WIFI_OMIT_RSSI) {
			dst->_ap_signalStrength._ap_signalStrength = src->rssi;
			dst->_ap_signalStrength_present = true;
		} else {
			dst->_ap_signalStrength_present = false;
		}
		if (src->channel != NRF_CLOUD_LOCATION_WIFI_OMIT_CHAN) {
			dst->_ap_channel._ap_channel = src->channel;
			dst->_ap_channel_present = true;
		} else {
			dst->_ap_channel_present = false;
		}
		dst->_ap_frequency_present = false;
		if (src->ssid_length && src->ssid[0]) {
			dst->_ap_ssid._ap_ssid.value = src->ssid;
			dst->_ap_ssid._ap_ssid.len = src->ssid_length;
			dst->_ap_ssid_present = true;
		} else {
			dst->_ap_ssid_present = false;
		}
		num_aps--;
		if (!num_aps) {
			break;
		}
	}
}

int coap_codec_ground_fix_req_encode(struct lte_lc_cells_info const *const cell_info,
				     struct wifi_scan_info const *const wifi_info,
				     uint8_t *buf, size_t *len, enum coap_content_format fmt)
{
	__ASSERT_NO_MSG((cell_info != NULL) || (wifi_info != NULL));
	__ASSERT_NO_MSG(buf != NULL);
	__ASSERT_NO_MSG(len != NULL);

	if (fmt != COAP_CONTENT_FORMAT_APP_CBOR) {
		LOG_ERR("Invalid format for ground fix: %d", fmt);
		return -ENOTSUP;
	}

	int err = 0;
	struct ground_fix_req input;
	size_t out_len;

	memset(&input, 0, sizeof(struct ground_fix_req));
	input._ground_fix_req_lte_present = (cell_info != NULL);
	if (cell_info) {
		copy_cell_info(&input._ground_fix_req_lte._ground_fix_req_lte, cell_info);
	}
	input._ground_fix_req_wifi_present = (wifi_info != NULL);
	if (wifi_info) {
		copy_wifi_info(&input._ground_fix_req_wifi._ground_fix_req_wifi, wifi_info);
	}
	err = cbor_encode_ground_fix_req(buf, *len, &input, &out_len);
	if (err) {
		LOG_ERR("Error %d encoding groundfix", err);
		err = -EINVAL;
		*len = 0;
	} else {
		*len = out_len;
	}
	return err;
}

int coap_codec_ground_fix_resp_decode(struct nrf_cloud_location_result *result,
				      const uint8_t *buf, size_t len, enum coap_content_format fmt)
{
	__ASSERT_NO_MSG(result != NULL);
	__ASSERT_NO_MSG(buf != NULL);
	int err;

	if (fmt == COAP_CONTENT_FORMAT_APP_JSON) {
		enum nrf_cloud_error nrf_err;

		/* Check for a potential ground fix JSON error message from nRF Cloud */
		err = nrf_cloud_error_msg_decode(buf, NRF_CLOUD_JSON_APPID_VAL_LOCATION,
						 NRF_CLOUD_JSON_MSG_TYPE_VAL_DATA, &nrf_err);
		if (!err) {
			LOG_ERR("nRF Cloud returned A-GPS error: %d", nrf_err);
			return -EFAULT;
		}
		LOG_ERR("Invalid A-GPS response format: %d", err);
		return -EPROTO;
	}
	if (fmt != COAP_CONTENT_FORMAT_APP_CBOR) {
		LOG_ERR("Invalid format for ground fix: %d", fmt);
		return -ENOTSUP;
	}

	struct ground_fix_resp res;
	size_t out_len;

	err = cbor_decode_ground_fix_resp(buf, len, &res, &out_len);
	if (out_len != len) {
		LOG_WRN("Different response length: expected:%zd, decoded:%zd",
			len, out_len);
	}
	if (err) {
		return err;
	}

	const char *with = res._ground_fix_resp_fulfilledWith.value;
	int rlen = res._ground_fix_resp_fulfilledWith.len;

	if (strncmp(with, NRF_CLOUD_LOCATION_TYPE_VAL_MCELL, rlen) == 0) {
		result->type = LOCATION_TYPE_MULTI_CELL;
	} else if (strncmp(with, NRF_CLOUD_LOCATION_TYPE_VAL_SCELL, rlen) == 0) {
		result->type = LOCATION_TYPE_SINGLE_CELL;
	} else if (strncmp(with, NRF_CLOUD_LOCATION_TYPE_VAL_WIFI, rlen) == 0) {
		result->type = LOCATION_TYPE_WIFI;
	} else {
		result->type = LOCATION_TYPE__INVALID;
		LOG_WRN("Unhandled location type: %*s", rlen, with);
	}

	result->lat = res._ground_fix_resp_lat;
	result->lon = res._ground_fix_resp_lon;

	if (res._ground_fix_resp_uncertainty_choice == _ground_fix_resp_uncertainty_int) {
		result->unc = res._ground_fix_resp_uncertainty_int;
	} else {
		result->unc = (uint32_t)lround(res._ground_fix_resp_uncertainty_float);
	}
	return err;
}

#if defined(CONFIG_NRF_CLOUD_AGPS)
#define ALL_TYPES (NRF_MODEM_GNSS_AGPS_GPS_UTC_REQUEST | \
		   NRF_MODEM_GNSS_AGPS_KLOBUCHAR_REQUEST | \
		   NRF_MODEM_GNSS_AGPS_NEQUICK_REQUEST | \
		   NRF_MODEM_GNSS_AGPS_SYS_TIME_AND_SV_TOW_REQUEST | \
		   NRF_MODEM_GNSS_AGPS_POSITION_REQUEST | \
		   NRF_MODEM_GNSS_AGPS_INTEGRITY_REQUEST)

static bool agps_all_types_set(struct nrf_modem_gnss_agps_data_frame const *const req)
{
	__ASSERT_NO_MSG(req != NULL);

	return ((req->sv_mask_ephe != 0) && (req->sv_mask_alm != 0) &&
		((req->data_flags & (ALL_TYPES)) == ALL_TYPES));
}

static int format_agps_custom_types_array(struct nrf_modem_gnss_agps_data_frame const *const req,
	int *array, int array_len)
{
	__ASSERT_NO_MSG(req != NULL);
	__ASSERT_NO_MSG(array != NULL);
	__ASSERT_NO_MSG(array_len >= (NRF_CLOUD_AGPS__LAST + 1));

	int pos = 0;

	if (req->data_flags & NRF_MODEM_GNSS_AGPS_GPS_UTC_REQUEST) {
		array[pos++] = NRF_CLOUD_AGPS_UTC_PARAMETERS;
	}
	if (req->sv_mask_ephe) {
		array[pos++] = NRF_CLOUD_AGPS_EPHEMERIDES;
	}
	if (req->sv_mask_alm) {
		array[pos++] = NRF_CLOUD_AGPS_ALMANAC;
	}
	if (req->data_flags & NRF_MODEM_GNSS_AGPS_KLOBUCHAR_REQUEST) {
		array[pos++] = NRF_CLOUD_AGPS_KLOBUCHAR_CORRECTION;
	}
	if (req->data_flags & NRF_MODEM_GNSS_AGPS_NEQUICK_REQUEST) {
		array[pos++] = NRF_CLOUD_AGPS_NEQUICK_CORRECTION;
	}
	if (req->data_flags & NRF_MODEM_GNSS_AGPS_SYS_TIME_AND_SV_TOW_REQUEST) {
		array[pos++] = NRF_CLOUD_AGPS_GPS_TOWS;
		array[pos++] = NRF_CLOUD_AGPS_GPS_SYSTEM_CLOCK;
	}
	if (req->data_flags & NRF_MODEM_GNSS_AGPS_POSITION_REQUEST) {
		array[pos++] = NRF_CLOUD_AGPS_LOCATION;
	}
	if (req->data_flags & NRF_MODEM_GNSS_AGPS_INTEGRITY_REQUEST) {
		array[pos++] = NRF_CLOUD_AGPS_INTEGRITY;
	}

	return pos;
}

int coap_codec_agps_encode(struct nrf_cloud_rest_agps_request const *const request,
			   uint8_t *buf, size_t *len, enum coap_content_format fmt)
{
	__ASSERT_NO_MSG(request != NULL);
	__ASSERT_NO_MSG(buf != NULL);
	__ASSERT_NO_MSG(len != NULL);

	if (fmt != COAP_CONTENT_FORMAT_APP_CBOR) {
		LOG_ERR("Invalid format for A-GPS: %d", fmt);
		return -ENOTSUP;
	}

	int ret;
	struct agps_req input;
	struct agps_req_types_ *t = &input._agps_req_types;
	size_t out_len;

	memset(&input, 0, sizeof(struct agps_req));
	input._agps_req_eci = request->net_info->current_cell.id;
	input._agps_req_mcc = request->net_info->current_cell.mcc;
	input._agps_req_mnc = request->net_info->current_cell.mnc;
	input._agps_req_tac = request->net_info->current_cell.tac;
	if (agps_all_types_set(request->agps_req) &&
	    (request->type == NRF_CLOUD_REST_AGPS_REQ_CUSTOM)) {
		input._agps_req_requestType._agps_req_requestType._type_choice =
			_type__rtAssistance;
		input._agps_req_requestType_present = true;
	} else if (request->type == NRF_CLOUD_REST_AGPS_REQ_CUSTOM) {
		t->_agps_req_types_int_count = format_agps_custom_types_array(
						request->agps_req,
						t->_agps_req_types_int,
						ARRAY_SIZE(t->_agps_req_types_int));
		input._agps_req_types_present = true;
		input._agps_req_requestType._agps_req_requestType._type_choice =
			_type__custom;
		input._agps_req_requestType_present = true;
	} else {
		input._agps_req_requestType._agps_req_requestType._type_choice =
			_type__rtAssistance;
		input._agps_req_requestType_present = true;
	}
	if (request->filtered) {
		input._agps_req_filtered_present = true;
		input._agps_req_filtered._agps_req_filtered = true;
		if (request->mask_angle != 5) {
			input._agps_req_mask_present = true;
			input._agps_req_mask._agps_req_mask = request->mask_angle;
		}
	}
	if (request->net_info->current_cell.rsrp != NRF_CLOUD_LOCATION_CELL_OMIT_RSRP) {
		input._agps_req_rsrp._agps_req_rsrp = request->net_info->current_cell.rsrp;
		input._agps_req_rsrp_present = true;
	}
	ret = cbor_encode_agps_req(buf, *len, &input, &out_len);
	if (ret) {
		LOG_ERR("Error %d encoding A-GPS req", ret);
		ret = -EINVAL;
		*len = 0;
	} else {
		*len = out_len;
	}
	return ret;
}

int coap_codec_agps_resp_decode(struct nrf_cloud_rest_agps_result *result,
				const uint8_t *buf, size_t len, enum coap_content_format fmt)
{
	__ASSERT_NO_MSG(result != NULL);
	__ASSERT_NO_MSG(buf != NULL);

	if (fmt == COAP_CONTENT_FORMAT_APP_JSON) {
		enum nrf_cloud_error nrf_err;
		int err;

		/* Check for a potential A-GPS JSON error message from nRF Cloud */
		err = nrf_cloud_error_msg_decode(buf, NRF_CLOUD_JSON_APPID_VAL_AGPS,
						 NRF_CLOUD_JSON_MSG_TYPE_VAL_DATA, &nrf_err);
		if (!err) {
			LOG_ERR("nRF Cloud returned A-GPS error: %d", nrf_err);
			return -EFAULT;
		}
		LOG_ERR("Invalid A-GPS response format");
		return -EPROTO;
	}
	if (fmt != COAP_CONTENT_FORMAT_APP_OCTET_STREAM) {
		LOG_ERR("Invalid format for A-GPS data: %d", fmt);
		return -ENOTSUP;
	}
	if (!result) {
		return -EINVAL;
	}

	if (result->buf_sz < len) {
		LOG_WRN("A-GPS buffer is too small; expected: %zd, got:%zd; truncated",
			len, result->buf_sz);
	}
	result->agps_sz = MIN(result->buf_sz, len);
	memcpy(result->buf, buf, result->agps_sz);
	return 0;
}
#endif /* CONFIG_NRF_CLOUD_AGPS */

#if defined(CONFIG_NRF_CLOUD_PGPS)
int coap_codec_pgps_encode(struct nrf_cloud_rest_pgps_request const *const request,
			   uint8_t *buf, size_t *len,
			   enum coap_content_format fmt)
{
	__ASSERT_NO_MSG(request != NULL);
	__ASSERT_NO_MSG(request->pgps_req != NULL);
	__ASSERT_NO_MSG(buf != NULL);
	__ASSERT_NO_MSG(len != NULL);

	if (fmt != COAP_CONTENT_FORMAT_APP_CBOR) {
		LOG_ERR("Invalid format for P-GPS: %d", fmt);
		return -ENOTSUP;
	}

	int ret;
	struct pgps_req input;
	size_t out_len;

	memset(&input, 0, sizeof(input));

	input._pgps_req_predictionCount = request->pgps_req->prediction_count;
	input._pgps_req_predictionIntervalMinutes = request->pgps_req->prediction_period_min;
	input._pgps_req_startGPSDay = request->pgps_req->gps_day;
	input._pgps_req_startGPSTimeOfDaySeconds = request->pgps_req->gps_time_of_day;

	ret = cbor_encode_pgps_req(buf, *len, &input, &out_len);
	if (ret) {
		LOG_ERR("Error %d encoding P-GPS req", ret);
		ret = -EINVAL;
		*len = 0;
	} else {
		*len = out_len;
	}

	return ret;
}

int coap_codec_pgps_resp_decode(struct nrf_cloud_pgps_result *result,
				const uint8_t *buf, size_t len, enum coap_content_format fmt)
{
	__ASSERT_NO_MSG(result != NULL);
	__ASSERT_NO_MSG(buf != NULL);
	int err;

	if (fmt == COAP_CONTENT_FORMAT_APP_JSON) {
		enum nrf_cloud_error nrf_err;

		/* Check for a potential P-GPS JSON error message from nRF Cloud */
		err = nrf_cloud_error_msg_decode(buf, NRF_CLOUD_JSON_APPID_VAL_PGPS,
						 NRF_CLOUD_JSON_MSG_TYPE_VAL_DATA, &nrf_err);
		if (!err) {
			LOG_ERR("nRF Cloud returned P-GPS error: %d", nrf_err);
			return -EFAULT;
		}
		LOG_ERR("Invalid P-GPS response format");
		return -EPROTO;
	}
	if (fmt != COAP_CONTENT_FORMAT_APP_CBOR) {
		LOG_ERR("Invalid format for P-GPS: %d", fmt);
		return -ENOTSUP;
	}

	struct pgps_resp resp;
	size_t resp_len;

	err = cbor_decode_pgps_resp(buf, len, &resp, &resp_len);
	if (!err) {
		strncpy(result->host, resp._pgps_resp_host.value, result->host_sz);
		if (result->host_sz > resp._pgps_resp_host.len) {
			result->host[resp._pgps_resp_host.len] = '\0';
		}
		result->host_sz = resp._pgps_resp_host.len;
		strncpy(result->path, resp._pgps_resp_path.value, result->path_sz);
		if (result->path_sz > resp._pgps_resp_path.len) {
			result->path[resp._pgps_resp_path.len] = '\0';
		}
		result->path_sz = resp._pgps_resp_path.len;
	}
	return err;
}
#endif /* CONFIG_NRF_CLOUD_PGPS */

int coap_codec_fota_resp_decode(struct nrf_cloud_fota_job_info *job,
				const uint8_t *buf, size_t len, enum coap_content_format fmt)
{
	__ASSERT_NO_MSG(job != NULL);
	__ASSERT_NO_MSG(buf != NULL);

	if (fmt == COAP_CONTENT_FORMAT_APP_CBOR) {
		LOG_ERR("Invalid format for FOTA: %d", fmt);
		return -ENOTSUP;
	} else {
		return nrf_cloud_rest_fota_execution_decode(buf, job);
	}
}
