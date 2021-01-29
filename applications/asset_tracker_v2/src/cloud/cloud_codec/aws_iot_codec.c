/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <cloud_codec.h>
#include <stdbool.h>
#include <string.h>
#include <zephyr.h>
#include <zephyr/types.h>
#include <stdio.h>
#include <stdlib.h>
#include "cJSON.h"
#include "json_aux.h"
#include <date_time.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(cloud_codec, CONFIG_CLOUD_CODEC_LOG_LEVEL);

#define MODEM_CURRENT_BAND	"band"
#define MODEM_NETWORK_MODE	"nw"
#define MODEM_ICCID		"iccid"
#define MODEM_FIRMWARE_VERSION	"modV"
#define MODEM_BOARD		"brdV"
#define MODEM_APP_VERSION	"appV"
#define MODEM_RSRP		"rsrp"
#define MODEM_AREA_CODE		"area"
#define MODEM_MCCMNC		"mccmnc"
#define MODEM_CELL_ID		"cell"
#define MODEM_IP_ADDRESS	"ip"

#define CONFIG_DEVICE_MODE	"act"
#define CONFIG_ACTIVE_TIMEOUT	"actwt"
#define CONFIG_MOVE_TIMEOUT	"mvt"
#define CONFIG_MOVE_RES		"mvres"
#define CONFIG_GPS_TIMEOUT	"gpst"
#define CONFIG_ACC_THRESHOLD	"acct"

#define OBJECT_CONFIG		"cfg"
#define OBJECT_REPORTED		"reported"
#define OBJECT_STATE		"state"
#define OBJECT_VALUE		"v"
#define OBJECT_TIMESTAMP	"ts"

#define DATA_MODEM_DYNAMIC	"roam"
#define DATA_MODEM_STATIC	"dev"
#define DATA_BATTERY		"bat"
#define DATA_TEMPERATURE	"temp"
#define DATA_HUMID		"hum"
#define DATA_ENVIRONMENTALS	"env"
#define DATA_BUTTON		"btn"

#define DATA_MOVEMENT		"acc"
#define DATA_MOVEMENT_X		"x"
#define DATA_MOVEMENT_Y		"y"
#define DATA_MOVEMENT_Z		"z"

#define DATA_GPS		"gps"
#define DATA_GPS_LONGITUDE	"lng"
#define DATA_GPS_LATITUDE	"lat"
#define DATA_GPS_ALTITUDE	"alt"
#define DATA_GPS_SPEED		"spd"
#define DATA_GPS_HEADING	"hdg"

#define MODEM_STATIC_QUEUE_ENTRY_COUNT		CONFIG_MODEM_BUFFER_STATIC_MAX
#define MODEM_STATIC_QUEUE_BYTE_ALIGNMENT	4

#define MODEM_DYNAM_QUEUE_ENTRY_COUNT		CONFIG_MODEM_BUFFER_DYNAMIC_MAX
#define MODEM_DYNAM_QUEUE_BYTE_ALIGNMENT	4

#define UI_QUEUE_ENTRY_COUNT			CONFIG_UI_BUFFER_MAX
#define UI_QUEUE_BYTE_ALIGNMENT			4

#define GPS_QUEUE_ENTRY_COUNT			CONFIG_GPS_BUFFER_MAX
#define GPS_QUEUE_BYTE_ALIGNMENT		4

#define SENSOR_QUEUE_ENTRY_COUNT		CONFIG_SENSOR_BUFFER_MAX
#define SENSOR_QUEUE_BYTE_ALIGNMENT		4

#define ACCEL_QUEUE_ENTRY_COUNT			CONFIG_ACCEL_BUFFER_MAX
#define ACCEL_QUEUE_BYTE_ALIGNMENT		4

#define BATTERY_QUEUE_ENTRY_COUNT		CONFIG_BATTERY_BUFFER_MAX
#define BATTERY_QUEUE_BYTE_ALIGNMENT		4

K_MSGQ_DEFINE(modem_stat_buf,
	      sizeof(struct cloud_data_modem_static),
	      MODEM_STATIC_QUEUE_ENTRY_COUNT,
	      MODEM_STATIC_QUEUE_BYTE_ALIGNMENT);

K_MSGQ_DEFINE(modem_dyn_buf,
	      sizeof(struct cloud_data_modem_dynamic),
	      MODEM_DYNAM_QUEUE_ENTRY_COUNT,
	      MODEM_DYNAM_QUEUE_BYTE_ALIGNMENT);

K_MSGQ_DEFINE(ui_buf,
	      sizeof(struct cloud_data_ui),
	      UI_QUEUE_ENTRY_COUNT,
	      UI_QUEUE_BYTE_ALIGNMENT);

K_MSGQ_DEFINE(gps_buf,
	      sizeof(struct cloud_data_gps),
	      GPS_QUEUE_ENTRY_COUNT,
	      GPS_QUEUE_BYTE_ALIGNMENT);

K_MSGQ_DEFINE(sensor_buf,
	      sizeof(struct cloud_data_sensors),
	      SENSOR_QUEUE_ENTRY_COUNT,
	      SENSOR_QUEUE_BYTE_ALIGNMENT);

K_MSGQ_DEFINE(accel_buf,
	      sizeof(struct cloud_data_accelerometer),
	      ACCEL_QUEUE_ENTRY_COUNT,
	      ACCEL_QUEUE_BYTE_ALIGNMENT);

K_MSGQ_DEFINE(battery_buf,
	      sizeof(struct cloud_data_battery),
	      BATTERY_QUEUE_ENTRY_COUNT,
	      BATTERY_QUEUE_BYTE_ALIGNMENT);

/* Static functions */
static int static_modem_data_add(cJSON *parent,
				 struct cloud_data_modem_static *data,
				 bool batch_entry)
{
	int err = 0;
	char nw_mode[50];

	static const char lte_string[] = "LTE-M";
	static const char nbiot_string[] = "NB-IoT";
	static const char gps_string[] = " GPS";

	err = date_time_uptime_to_unix_time_ms(&data->ts);
	if (err) {
		LOG_ERR("date_time_uptime_to_unix_time_ms, error: %d", err);
		return err;
	}

	cJSON *static_m = cJSON_CreateObject();
	cJSON *static_m_v = cJSON_CreateObject();

	if (static_m == NULL || static_m_v == NULL) {
		cJSON_Delete(static_m);
		cJSON_Delete(static_m_v);
		return -ENOMEM;
	}

	if (data->nw_lte_m) {
		strcpy(nw_mode, lte_string);
	} else if (data->nw_nb_iot) {
		strcpy(nw_mode, nbiot_string);
	}

	if (data->nw_gps) {
		strcat(nw_mode, gps_string);
	}

	err += json_add_number(static_m_v, MODEM_CURRENT_BAND, data->bnd);
	err += json_add_str(static_m_v, MODEM_NETWORK_MODE, nw_mode);
	err += json_add_str(static_m_v, MODEM_ICCID, data->iccid);
	err += json_add_str(static_m_v, MODEM_FIRMWARE_VERSION, data->fw);
	err += json_add_str(static_m_v, MODEM_BOARD, data->brdv);
	err += json_add_str(static_m_v, MODEM_APP_VERSION, data->appv);

	err += json_add_obj(static_m, OBJECT_VALUE, static_m_v);
	err += json_add_number(static_m, OBJECT_TIMESTAMP, data->ts);

	if (batch_entry) {
		err += json_add_obj_array(parent, static_m);
	} else {
		err += json_add_obj(parent, DATA_MODEM_STATIC, static_m);
	}

	if (err) {
		return err;
	}

	return 0;
}

static int dynamic_modem_data_add(cJSON *parent,
				  struct cloud_data_modem_dynamic *data,
				  bool batch_entry)
{
	int err = 0;
	long mccmnc;

	err = date_time_uptime_to_unix_time_ms(&data->ts);
	if (err) {
		LOG_ERR("date_time_uptime_to_unix_time_ms, error: %d", err);
		return err;
	}

	cJSON *dynamic_m = cJSON_CreateObject();
	cJSON *dynamic_m_v = cJSON_CreateObject();

	if (dynamic_m == NULL || dynamic_m_v == NULL) {
		cJSON_Delete(dynamic_m);
		cJSON_Delete(dynamic_m_v);
		return -ENOMEM;
	}

	mccmnc = strtol(data->mccmnc, NULL, 10);

	err += json_add_number(dynamic_m_v, MODEM_RSRP, data->rsrp);
	err += json_add_number(dynamic_m_v, MODEM_AREA_CODE, data->area);
	err += json_add_number(dynamic_m_v, MODEM_MCCMNC, mccmnc);
	err += json_add_number(dynamic_m_v, MODEM_CELL_ID, data->cell);
	err += json_add_str(dynamic_m_v, MODEM_IP_ADDRESS, data->ip);

	err += json_add_obj(dynamic_m, OBJECT_VALUE, dynamic_m_v);
	err += json_add_number(dynamic_m, OBJECT_TIMESTAMP, data->ts);

	if (batch_entry) {
		err += json_add_obj_array(parent, dynamic_m);
	} else {
		err += json_add_obj(parent, DATA_MODEM_DYNAMIC, dynamic_m);
	}

	if (err) {
		return err;
	}

	return 0;
}

static int sensor_data_add(cJSON *parent, struct cloud_data_sensors *data,
			   bool batch_entry)
{
	int err = 0;

	err = date_time_uptime_to_unix_time_ms(&data->env_ts);
	if (err) {
		LOG_ERR("date_time_uptime_to_unix_time_ms, error: %d", err);
		return err;
	}

	cJSON *sensor_obj = cJSON_CreateObject();
	cJSON *sensor_val_obj = cJSON_CreateObject();

	if (sensor_obj == NULL || sensor_val_obj == NULL) {
		cJSON_Delete(sensor_obj);
		cJSON_Delete(sensor_val_obj);
		return -ENOMEM;
	}

	err = json_add_number(sensor_val_obj, DATA_TEMPERATURE, data->temp);
	err += json_add_number(sensor_val_obj, DATA_HUMID, data->hum);
	err += json_add_obj(sensor_obj, OBJECT_VALUE, sensor_val_obj);
	err += json_add_number(sensor_obj, OBJECT_TIMESTAMP, data->env_ts);

	if (batch_entry) {
		err += json_add_obj_array(parent, sensor_obj);
	} else {
		err += json_add_obj(parent, DATA_ENVIRONMENTALS, sensor_obj);
	}

	if (err) {
		return err;
	}

	return 0;
}

static int gps_data_add(cJSON *parent, struct cloud_data_gps *data,
			bool batch_entry)
{
	int err = 0;

	err = date_time_uptime_to_unix_time_ms(&data->gps_ts);
	if (err) {
		LOG_ERR("date_time_uptime_to_unix_time_ms, error: %d", err);
		return err;
	}

	cJSON *gps_obj = cJSON_CreateObject();
	cJSON *gps_val_obj = cJSON_CreateObject();

	if (gps_obj == NULL || gps_val_obj == NULL) {
		cJSON_Delete(gps_obj);
		cJSON_Delete(gps_val_obj);
		return -ENOMEM;
	}

	err += json_add_number(gps_val_obj, DATA_GPS_LONGITUDE, data->longi);
	err += json_add_number(gps_val_obj, DATA_GPS_LATITUDE, data->lat);
	err += json_add_number(gps_val_obj, DATA_MOVEMENT, data->acc);
	err += json_add_number(gps_val_obj, DATA_GPS_ALTITUDE, data->alt);
	err += json_add_number(gps_val_obj, DATA_GPS_SPEED, data->spd);
	err += json_add_number(gps_val_obj, DATA_GPS_HEADING, data->hdg);

	err += json_add_obj(gps_obj, OBJECT_VALUE, gps_val_obj);
	err += json_add_number(gps_obj, OBJECT_TIMESTAMP, data->gps_ts);

	if (batch_entry) {
		err += json_add_obj_array(parent, gps_obj);
	} else {
		err += json_add_obj(parent, DATA_GPS, gps_obj);
	}

	if (err) {
		return err;
	}

	return 0;
}

static int accel_data_add(cJSON *parent, struct cloud_data_accelerometer *data,
			  bool batch_entry)
{
	int err = 0;

	err = date_time_uptime_to_unix_time_ms(&data->ts);
	if (err) {
		LOG_ERR("date_time_uptime_to_unix_time_ms, error: %d", err);
		return err;
	}

	cJSON *acc_obj = cJSON_CreateObject();
	cJSON *acc_v_obj = cJSON_CreateObject();

	if (acc_obj == NULL || acc_v_obj == NULL) {
		cJSON_Delete(acc_obj);
		cJSON_Delete(acc_v_obj);
		return -ENOMEM;
	}

	err += json_add_number(acc_v_obj, DATA_MOVEMENT_X, data->values[0]);
	err += json_add_number(acc_v_obj, DATA_MOVEMENT_Y, data->values[1]);
	err += json_add_number(acc_v_obj, DATA_MOVEMENT_Z, data->values[2]);

	err += json_add_obj(acc_obj, OBJECT_VALUE, acc_v_obj);
	err += json_add_number(acc_obj, OBJECT_TIMESTAMP, data->ts);

	if (batch_entry) {
		err += json_add_obj_array(parent, acc_obj);
	} else {
		err += json_add_obj(parent, DATA_MOVEMENT, acc_obj);
	}

	if (err) {
		return err;
	}

	return 0;
}

static int ui_data_add(cJSON *parent, struct cloud_data_ui *data,
		       bool batch_entry)
{
	int err = 0;

	err = date_time_uptime_to_unix_time_ms(&data->btn_ts);
	if (err) {
		LOG_ERR("date_time_uptime_to_unix_time_ms, error: %d", err);
		return err;
	}

	cJSON *btn_obj = cJSON_CreateObject();

	if (btn_obj == NULL) {
		cJSON_Delete(btn_obj);
		return -ENOMEM;
	}

	err += json_add_number(btn_obj, OBJECT_VALUE, data->btn);
	err += json_add_number(btn_obj, OBJECT_TIMESTAMP, data->btn_ts);

	if (batch_entry) {
		err += json_add_obj_array(parent, btn_obj);
	} else {
		err += json_add_obj(parent, DATA_BUTTON, btn_obj);
	}

	if (err) {
		return err;
	}

	return 0;
}

static int battery_data_add(cJSON *parent, struct cloud_data_battery *data,
			    bool batch_entry)
{
	int err = 0;

	err = date_time_uptime_to_unix_time_ms(&data->bat_ts);
	if (err) {
		LOG_ERR("date_time_uptime_to_unix_time_ms, error: %d", err);
		return err;
	}

	cJSON *bat_obj = cJSON_CreateObject();

	if (bat_obj == NULL) {
		cJSON_Delete(bat_obj);
		return -ENOMEM;
	}

	err += json_add_number(bat_obj, OBJECT_VALUE, data->bat);
	err += json_add_number(bat_obj, OBJECT_TIMESTAMP, data->bat_ts);

	if (batch_entry) {
		err += json_add_obj_array(parent, bat_obj);
	} else {
		err += json_add_obj(parent, DATA_BATTERY, bat_obj);
	}

	if (err) {
		return err;
	}

	return 0;
}

/* Public interface */
int cloud_codec_decode_config(char *input, struct cloud_data_cfg *data)
{
	int err = 0;
	cJSON *root_obj = NULL;
	cJSON *group_obj = NULL;
	cJSON *subgroup_obj = NULL;
	cJSON *gps_timeout = NULL;
	cJSON *active = NULL;
	cJSON *active_wait = NULL;
	cJSON *move_res = NULL;
	cJSON *move_timeout = NULL;
	cJSON *acc_thres = NULL;

	if (input == NULL) {
		return -EINVAL;
	}

	root_obj = cJSON_Parse(input);
	if (root_obj == NULL) {
		return -ENOENT;
	}

	if (IS_ENABLED(CONFIG_CLOUD_CODEC_LOG_LEVEL_DBG)) {
		json_print_obj("Decoded message:\n", root_obj);
	}

	group_obj = json_object_decode(root_obj, OBJECT_CONFIG);
	if (group_obj != NULL) {
		subgroup_obj = group_obj;
		goto get_data;
	}

	group_obj = json_object_decode(root_obj, OBJECT_STATE);
	if (group_obj == NULL) {
		err = -ENODATA;
		goto exit;
	}

	subgroup_obj = json_object_decode(group_obj, OBJECT_CONFIG);
	if (subgroup_obj == NULL) {
		err = -ENODATA;
		goto exit;
	}

get_data:

	gps_timeout = cJSON_GetObjectItem(subgroup_obj, CONFIG_GPS_TIMEOUT);
	active = cJSON_GetObjectItem(subgroup_obj, CONFIG_DEVICE_MODE);
	active_wait = cJSON_GetObjectItem(subgroup_obj, CONFIG_ACTIVE_TIMEOUT);
	move_res = cJSON_GetObjectItem(subgroup_obj, CONFIG_MOVE_RES);
	move_timeout = cJSON_GetObjectItem(subgroup_obj, CONFIG_MOVE_TIMEOUT);
	acc_thres = cJSON_GetObjectItem(subgroup_obj, CONFIG_ACC_THRESHOLD);

	if (gps_timeout != NULL) {
		data->gps_timeout = gps_timeout->valueint;
	}

	if (active != NULL) {
		data->active_mode = active->valueint;
	}

	if (active_wait != NULL) {
		data->active_wait_timeout = active_wait->valueint;
	}

	if (move_res != NULL) {
		data->movement_resolution = move_res->valueint;
	}

	if (move_timeout != NULL) {
		data->movement_timeout = move_timeout->valueint;
	}

	if (acc_thres != NULL) {
		data->accelerometer_threshold = acc_thres->valuedouble;
	}

exit:
	cJSON_Delete(root_obj);
	return err;
}

int cloud_codec_encode_config(struct cloud_codec_data *output,
			      struct cloud_data_cfg *data)
{
	int err = 0;
	char *buffer;

	cJSON *root_obj = cJSON_CreateObject();
	cJSON *state_obj = cJSON_CreateObject();
	cJSON *rep_obj = cJSON_CreateObject();
	cJSON *cfg_obj = cJSON_CreateObject();

	if (root_obj == NULL || state_obj == NULL || rep_obj == NULL ||
	    cfg_obj == NULL) {
		cJSON_Delete(root_obj);
		cJSON_Delete(state_obj);
		cJSON_Delete(rep_obj);
		cJSON_Delete(cfg_obj);
		return -ENOMEM;
	}

	err += json_add_bool(cfg_obj, CONFIG_DEVICE_MODE,
			     data->active_mode);
	err += json_add_number(cfg_obj, CONFIG_GPS_TIMEOUT,
			       data->gps_timeout);
	err += json_add_number(cfg_obj, CONFIG_ACTIVE_TIMEOUT,
			       data->active_wait_timeout);
	err += json_add_number(cfg_obj, CONFIG_MOVE_RES,
			       data->movement_resolution);
	err += json_add_number(cfg_obj, CONFIG_MOVE_TIMEOUT,
			       data->movement_timeout);
	err += json_add_number(cfg_obj, CONFIG_ACC_THRESHOLD,
			       data->accelerometer_threshold);

	err += json_add_obj(rep_obj, OBJECT_CONFIG, cfg_obj);
	err += json_add_obj(state_obj, OBJECT_REPORTED, rep_obj);
	err += json_add_obj(root_obj, OBJECT_STATE, state_obj);

	if (err) {
		goto exit;
	}

	buffer = cJSON_PrintUnformatted(root_obj);

	if (IS_ENABLED(CONFIG_CLOUD_CODEC_LOG_LEVEL_DBG)) {
		json_print_obj("Encoded message:\n", root_obj);
	}

	output->buf = buffer;
	output->len = strlen(buffer);

exit:
	cJSON_Delete(root_obj);
	return err;
}

int cloud_codec_encode_data(struct cloud_codec_data *output)
{
	int err = 0;
	int ret;
	char *buffer;
	bool data_encoded = false;

	cJSON *root_obj = cJSON_CreateObject();
	cJSON *state_obj = cJSON_CreateObject();
	cJSON *rep_obj = cJSON_CreateObject();

	struct cloud_data_ui ui;
	struct cloud_data_modem_static modem_static;
	struct cloud_data_modem_dynamic modem_dynamic;
	struct cloud_data_accelerometer accelerometer;
	struct cloud_data_battery battery;
	struct cloud_data_gps gps;
	struct cloud_data_sensors sensor;

	if (root_obj == NULL || state_obj == NULL || rep_obj == NULL) {
		cJSON_Delete(root_obj);
		cJSON_Delete(state_obj);
		cJSON_Delete(rep_obj);
		return -ENOMEM;
	}

	ret = k_msgq_get(&modem_stat_buf, &modem_static, K_NO_WAIT);
	if (ret == 0) {
		err = static_modem_data_add(rep_obj, &modem_static, false);
		if (err) {
			LOG_ERR("static_modem_data_add, error: %d", err);
			return err;
		}
		data_encoded = true;
	}

	ret = k_msgq_get(&modem_dyn_buf, &modem_dynamic, K_NO_WAIT);
	if (ret == 0) {
		err = dynamic_modem_data_add(rep_obj, &modem_dynamic, false);
		if (err) {
			LOG_ERR("dynamic_modem_data_add, error: %d", err);
			return err;
		}
		data_encoded = true;
	}

	ret = k_msgq_get(&ui_buf, &ui, K_NO_WAIT);
	if (ret == 0) {
		err = ui_data_add(rep_obj, &ui, false);
		if (err) {
			LOG_ERR("ui_data_add, error: %d", err);
			return err;
		}
		data_encoded = true;
	}

	ret = k_msgq_get(&accel_buf, &accelerometer, K_NO_WAIT);
	if (ret == 0) {
		err = accel_data_add(rep_obj, &accelerometer, false);
		if (err) {
			LOG_ERR("accel_data_add, error: %d", err);
			return err;
		}
		data_encoded = true;
	}

	ret = k_msgq_get(&battery_buf, &battery, K_NO_WAIT);
	if (ret == 0) {
		err = battery_data_add(rep_obj, &battery, false);
		if (err) {
			LOG_ERR("battery_data_add, error: %d", err);
			return err;
		}
		data_encoded = true;
	}

	ret = k_msgq_get(&gps_buf, &gps, K_NO_WAIT);
	if (ret == 0) {
		err = gps_data_add(rep_obj, &gps, false);
		if (err) {
			LOG_ERR("gps_data_add, error: %d", err);
			return err;
		}
		data_encoded = true;
	}

	ret = k_msgq_get(&sensor_buf, &sensor, K_NO_WAIT);
	if (ret == 0) {
		err = sensor_data_add(rep_obj, &sensor, false);
		if (err) {
			LOG_ERR("sensor_data_add, error: %d", err);
			return err;
		}
		data_encoded = true;
	}

	err += json_add_obj(state_obj, OBJECT_REPORTED, rep_obj);
	err += json_add_obj(root_obj, OBJECT_STATE, state_obj);

	/* Exit upon encoding errors or no data encoded. */
	if (err) {
		goto exit;
	}

	if (!data_encoded) {
		err = -ENODATA;
		LOG_DBG("No data to encode...");
		goto exit;
	}

	buffer = cJSON_PrintUnformatted(root_obj);

	if (IS_ENABLED(CONFIG_CLOUD_CODEC_LOG_LEVEL_DBG)) {
		json_print_obj("Encoded message:\n", root_obj);
	}

	output->buf = buffer;
	output->len = strlen(buffer);

exit:
	cJSON_Delete(root_obj);

	return err;
}

int cloud_codec_encode_ui_data(struct cloud_codec_data *output)
{
	int err = 0;
	int ret;
	char *buffer;

	cJSON *root_obj = cJSON_CreateObject();

	struct cloud_data_ui ui;

	if (root_obj == NULL) {
		cJSON_Delete(root_obj);
		return -ENOMEM;
	}

	ret = k_msgq_get(&ui_buf, &ui, K_NO_WAIT);
	if (ret == 0) {
		err = ui_data_add(root_obj, &ui, false);
		if (err) {
			LOG_ERR("ui_data_add, error: %d", err);
			goto exit;
		}
	} else {
		err = -ENODATA;
		goto exit;
	}

	buffer = cJSON_PrintUnformatted(root_obj);

	if (IS_ENABLED(CONFIG_CLOUD_CODEC_LOG_LEVEL_DBG)) {
		json_print_obj("Encoded message:\n", root_obj);
	}

	output->buf = buffer;
	output->len = strlen(buffer);

exit:
	cJSON_Delete(root_obj);

	return err;
}

int cloud_codec_encode_batch_data(struct cloud_codec_data *output)
{
	int err = 0;
	int ret;
	char *buffer;
	bool data_encoded = false;

	cJSON *root_obj = cJSON_CreateObject();
	cJSON *gps_obj = cJSON_CreateArray();
	cJSON *sensor_obj = cJSON_CreateArray();
	cJSON *modem_dyn_obj = cJSON_CreateArray();
	cJSON *modem_stat_obj = cJSON_CreateArray();
	cJSON *ui_obj = cJSON_CreateArray();
	cJSON *accel_obj = cJSON_CreateArray();
	cJSON *bat_obj = cJSON_CreateArray();

	if (root_obj == NULL || gps_obj == NULL || sensor_obj == NULL ||
	    modem_dyn_obj == NULL || modem_stat_obj == NULL || ui_obj == NULL ||
	    accel_obj == NULL || bat_obj == NULL) {
		cJSON_Delete(root_obj);
		cJSON_Delete(gps_obj);
		cJSON_Delete(sensor_obj);
		cJSON_Delete(modem_stat_obj);
		cJSON_Delete(modem_dyn_obj);
		cJSON_Delete(ui_obj);
		cJSON_Delete(accel_obj);
		cJSON_Delete(bat_obj);
		return -ENOMEM;
	}

	struct cloud_data_ui ui;
	struct cloud_data_modem_static modem_static;
	struct cloud_data_modem_dynamic modem_dynamic;
	struct cloud_data_battery battery;
	struct cloud_data_accelerometer accelerometer;
	struct cloud_data_gps gps;
	struct cloud_data_sensors sensor;

	/* UI */
	while (k_msgq_num_used_get(&ui_buf)) {
		ret = k_msgq_get(&ui_buf, &ui, K_NO_WAIT);
		if (ret == 0) {
			err = ui_data_add(ui_obj, &ui, true);
			if (err) {
				LOG_ERR("ui_data_add, error: %d", err);
				return err;
			}
		}
	}

	if (cJSON_GetArraySize(ui_obj) > 0) {
		err += json_add_obj(root_obj, DATA_BUTTON, ui_obj);
		data_encoded = true;
	} else {
		cJSON_Delete(ui_obj);
	}

	/* Modem static */
	while (k_msgq_num_used_get(&modem_stat_buf)) {
		ret = k_msgq_get(&modem_stat_buf, &modem_static, K_NO_WAIT);
		if (ret == 0) {
			err = static_modem_data_add(modem_stat_obj,
						    &modem_static,
						    true);
			if (err) {
				LOG_ERR("static_modem_data_add, error: %d",
					err);
				return err;
			}
			data_encoded = true;
		}
	}

	if (cJSON_GetArraySize(modem_stat_obj) > 0) {
		err += json_add_obj(root_obj,
				    DATA_MODEM_STATIC,
				    modem_stat_obj);
		data_encoded = true;
	} else {
		cJSON_Delete(modem_stat_obj);
	}

	/* Modem dynamic */
	while (k_msgq_num_used_get(&modem_dyn_buf)) {
		ret = k_msgq_get(&modem_dyn_buf, &modem_dynamic, K_NO_WAIT);
		if (ret == 0) {
			err = dynamic_modem_data_add(modem_dyn_obj,
						     &modem_dynamic,
						     true);
			if (err) {
				LOG_ERR("dynamic_modem_data_add, error: %d",
					err);
				return err;
			}
		}
	}

	if (cJSON_GetArraySize(modem_dyn_obj) > 0) {
		err += json_add_obj(root_obj,
				    DATA_MODEM_DYNAMIC,
				    modem_dyn_obj);
		data_encoded = true;
	} else {
		cJSON_Delete(modem_dyn_obj);
	}

	/* Battery */
	while (k_msgq_num_used_get(&battery_buf)) {
		ret = k_msgq_get(&battery_buf, &battery, K_NO_WAIT);
		if (ret == 0) {
			err = battery_data_add(bat_obj, &battery, true);
			if (err) {
				LOG_ERR("battery_data_add, error: %d", err);
				return err;
			}
		}
	}

	if (cJSON_GetArraySize(bat_obj) > 0) {
		err += json_add_obj(root_obj, DATA_BATTERY, bat_obj);
		data_encoded = true;
	} else {
		cJSON_Delete(bat_obj);
	}

	/* Accelerometer */
	while (k_msgq_num_used_get(&accel_buf)) {
		ret = k_msgq_get(&accel_buf, &accelerometer, K_NO_WAIT);
		if (ret == 0) {
			err = accel_data_add(accel_obj, &accelerometer, true);
			if (err) {
				LOG_ERR("accel_data_add, error: %d", err);
				return err;
			}
			data_encoded = true;
		}
	}

	if (cJSON_GetArraySize(accel_obj) > 0) {
		err += json_add_obj(root_obj, DATA_MOVEMENT, accel_obj);
		data_encoded = true;
	} else {
		cJSON_Delete(accel_obj);
	}


	/* GPS */
	while (k_msgq_num_used_get(&gps_buf)) {
		ret = k_msgq_get(&gps_buf, &gps, K_NO_WAIT);
		if (ret == 0) {
			err = gps_data_add(gps_obj, &gps, true);
			if (err) {
				LOG_ERR("gps_data_add, error: %d", err);
				return err;
			}
			data_encoded = true;
		}
	}

	if (cJSON_GetArraySize(gps_obj) > 0) {
		err += json_add_obj(root_obj, DATA_GPS, gps_obj);
		data_encoded = true;
	} else {
		cJSON_Delete(gps_obj);
	}

	/* Sensor */
	while (k_msgq_num_used_get(&sensor_buf)) {
		ret = k_msgq_get(&sensor_buf, &sensor, K_NO_WAIT);
		if (ret == 0) {
			err = sensor_data_add(sensor_obj, &sensor, true);
			if (err) {
				LOG_ERR("sensor_data_add, error: %d", err);
				return err;
			}
			data_encoded = true;
		}
	}

	if (cJSON_GetArraySize(sensor_obj) > 0) {
		err += json_add_obj(root_obj, DATA_ENVIRONMENTALS, sensor_obj);
		data_encoded = true;
	} else {
		cJSON_Delete(sensor_obj);
	}

	if (err) {
		goto exit;
	} else if (!data_encoded) {
		err = -ENODATA;
		goto exit;
	}

	buffer = cJSON_PrintUnformatted(root_obj);

	if (IS_ENABLED(CONFIG_CLOUD_CODEC_LOG_LEVEL_DBG)) {
		json_print_obj("Encoded batch message:\n", root_obj);
	}

	output->buf = buffer;
	output->len = strlen(buffer);

exit:

	cJSON_Delete(root_obj);

	return err;
}

int cloud_codec_enqueue_accel_data(
		struct cloud_data_accelerometer *new_accel_data)
{
	int err;
	struct cloud_data_accelerometer accel;

	if (k_msgq_num_free_get(&accel_buf) == 0) {
		/* Remove the oldest entry. */
		err = k_msgq_get(&accel_buf, &accel, K_NO_WAIT);
		if (err) {
			return err;
		}

		LOG_WRN("Oldest entry in accelerometer queue replaced");
	}

	err = k_msgq_put(&accel_buf, new_accel_data, K_NO_WAIT);
	if (err) {
		LOG_WRN("k_msgq_put, error %d", err);
		return err;
	}

	return 0;
}

int cloud_codec_enqueue_bat_data(struct cloud_data_battery *new_bat_data)
{
	int err;
	struct cloud_data_battery battery;

	if (k_msgq_num_free_get(&battery_buf) == 0) {
		/* Remove the oldest entry. */
		err = k_msgq_get(&battery_buf, &battery, K_NO_WAIT);
		if (err) {
			return err;
		}

		LOG_WRN("Oldest entry in battery queue replaced");
	}

	err = k_msgq_put(&battery_buf, new_bat_data, K_NO_WAIT);
	if (err) {
		LOG_WRN("k_msgq_put, error %d", err);
		return err;
	}

	return 0;
}

int cloud_codec_enqueue_gps_data(
		struct cloud_data_gps *new_gps_data)
{
	int err;
	struct cloud_data_gps gps;

	if (k_msgq_num_free_get(&gps_buf) == 0) {
		/* Remove the oldest entry. */
		err = k_msgq_get(&gps_buf, &gps, K_NO_WAIT);
		if (err) {
			return err;
		}

		LOG_WRN("Oldest entry in GPS queue replaced");
	}

	err = k_msgq_put(&gps_buf, new_gps_data, K_NO_WAIT);
	if (err) {
		LOG_WRN("k_msgq_put, error %d", err);
		return err;
	}

	return 0;
}

int cloud_codec_enqueue_modem_dynamic_data(
		struct cloud_data_modem_dynamic *new_dynamic_modem_data)
{
	int err;
	struct cloud_data_modem_dynamic modem_dynamic;

	if (k_msgq_num_free_get(&modem_dyn_buf) == 0) {
		/* Remove the oldest entry. */
		err = k_msgq_get(&modem_dyn_buf, &modem_dynamic, K_NO_WAIT);
		if (err) {
			return err;
		}

		LOG_WRN("Oldest entry in modem dynamic queue replaced");
	}

	err = k_msgq_put(&modem_dyn_buf, new_dynamic_modem_data, K_NO_WAIT);
	if (err) {
		LOG_WRN("k_msgq_put, error %d", err);
		return err;
	}

	return 0;
}

int cloud_codec_enqueue_modem_static_data(
		struct cloud_data_modem_static *new_modem_static_data)
{
	int err;
	struct cloud_data_modem_static modem_static;

	if (k_msgq_num_free_get(&modem_stat_buf) == 0) {
		/* Remove the oldest entry. */
		err = k_msgq_get(&modem_stat_buf, &modem_static, K_NO_WAIT);
		if (err) {
			return err;
		}

		LOG_WRN("Oldest entry in modem static queue replaced");
	}

	err = k_msgq_put(&modem_stat_buf, new_modem_static_data, K_NO_WAIT);
	if (err) {
		LOG_WRN("k_msgq_put, error %d", err);
		return err;
	}

	return 0;
}

int cloud_codec_enqueue_ui_data(struct cloud_data_ui *new_ui_data)
{
	int err;
	struct cloud_data_ui ui;

	if (k_msgq_num_free_get(&ui_buf) == 0) {
		/* Remove the oldest entry. */
		err = k_msgq_get(&ui_buf, &ui, K_NO_WAIT);
		if (err) {
			return err;
		}

		LOG_WRN("Oldest entry in UI queue replaced");
	}

	err = k_msgq_put(&ui_buf, new_ui_data, K_NO_WAIT);
	if (err) {
		LOG_WRN("k_msgq_put, error %d", err);
		return err;
	}

	return 0;
}

int cloud_codec_enqueue_sensor_data(struct cloud_data_sensors *new_sensor_data)
{
	int err;
	struct cloud_data_sensors sensor;

	if (k_msgq_num_free_get(&sensor_buf) == 0) {
		/* Remove the oldest entry. */
		err = k_msgq_get(&sensor_buf, &sensor, K_NO_WAIT);
		if (err) {
			return err;
		}

		LOG_WRN("Oldest entry in sensor queue replaced");
	}

	err = k_msgq_put(&sensor_buf, new_sensor_data, K_NO_WAIT);
	if (err) {
		LOG_WRN("k_msgq_put, error %d", err);
		return err;
	}

	return 0;
}
