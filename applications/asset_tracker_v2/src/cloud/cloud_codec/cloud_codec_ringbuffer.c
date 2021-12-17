/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <cloud_codec.h>
#include <zephyr.h>
#include <cJSON.h>
#include <cJSON_os.h>
#include <math.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(cloud_codec_ringbuffer, CONFIG_CLOUD_CODEC_LOG_LEVEL);

/* Ringbuffers. All data received by the Data module are stored in ringbuffers.
 * Upon a LTE connection loss the device will keep sampling/storing data in
 * the buffers, and empty the buffers in batches upon a reconnect.
 */
static struct cloud_data_gnss gnss_buffer[CONFIG_DATA_GNSS_BUFFER_COUNT];
static struct cloud_data_sensors sensors_buffer[CONFIG_DATA_SENSOR_BUFFER_COUNT];
static struct cloud_data_ui ui_buffer[CONFIG_DATA_UI_BUFFER_COUNT];
static struct cloud_data_accelerometer accel_buffer[CONFIG_DATA_ACCELEROMETER_BUFFER_COUNT];
static struct cloud_data_battery battery_buffer[CONFIG_DATA_BATTERY_BUFFER_COUNT];
static struct cloud_data_modem_dynamic modem_dyn_buf[CONFIG_DATA_MODEM_DYNAMIC_BUFFER_COUNT];
static struct cloud_data_neighbor_cells neighbor_cells;
static struct cloud_data_modem_static modem_stat;

/* Head of ringbuffers. */
static int head_gnss_buf;
static int head_sensor_buf;
static int head_modem_dyn_buf;
static int head_ui_buf;
static int head_accel_buf;
static int head_bat_buf;

void cloud_codec_populate_sensor_buffer(struct cloud_data_sensors *new_sensor_data)
{
	if (!IS_ENABLED(CONFIG_DATA_SENSOR_BUFFER_STORE)) {
		return;
	}

	if (!new_sensor_data->queued) {
		return;
	}

	/* Go to start of buffer if end is reached. */
	head_sensor_buf += 1;
	if (head_sensor_buf == ARRAY_SIZE(sensors_buffer)) {
		head_sensor_buf = 0;
	}

	sensors_buffer[head_sensor_buf] = *new_sensor_data;

	LOG_DBG("Entry: %d of %ld in sensor buffer filled", head_sensor_buf,
		ARRAY_SIZE(sensors_buffer) - 1);
}

void cloud_codec_populate_ui_buffer(struct cloud_data_ui *new_ui_data)
{
	if (!IS_ENABLED(CONFIG_DATA_UI_BUFFER_STORE)) {
		return;
	}

	if (!new_ui_data->queued) {
		return;
	}

	/* Go to start of buffer if end is reached. */
	head_ui_buf += 1;
	if (head_ui_buf == ARRAY_SIZE(ui_buffer)) {
		head_ui_buf = 0;
	}

	ui_buffer[head_ui_buf] = *new_ui_data;

	LOG_DBG("Entry: %d of %ld in UI buffer filled", head_ui_buf,
		ARRAY_SIZE(ui_buffer) - 1);
}

void cloud_codec_populate_accel_buffer(struct cloud_data_accelerometer *new_accel_data)
{
	if (!IS_ENABLED(CONFIG_DATA_ACCELEROMETER_BUFFER_STORE)) {
		return;
	}

	if (!new_accel_data->queued) {
		return;
	}

	/* Go to start of buffer if end is reached. */
	head_accel_buf += 1;
	if (head_accel_buf == ARRAY_SIZE(accel_buffer)) {
		head_accel_buf = 0;
	}

	accel_buffer[head_accel_buf] = *new_accel_data;

	LOG_DBG("Entry: %d of %ld in movement buffer filled", head_accel_buf,
		ARRAY_SIZE(accel_buffer) - 1);
}

void cloud_codec_populate_bat_buffer(struct cloud_data_battery *new_bat_data)
{
	if (!IS_ENABLED(CONFIG_DATA_BATTERY_BUFFER_STORE)) {
		return;
	}

	if (!new_bat_data->queued) {
		return;
	}

	/* Go to start of buffer if end is reached. */
	head_bat_buf += 1;
	if (head_bat_buf == ARRAY_SIZE(battery_buffer)) {
		head_bat_buf = 0;
	}

	battery_buffer[head_bat_buf] = *new_bat_data;

	LOG_DBG("Entry: %d of %ld in battery buffer filled", head_bat_buf,
		ARRAY_SIZE(battery_buffer) - 1);
}

void cloud_codec_populate_gnss_buffer(struct cloud_data_gnss *new_gnss_data)
{
	if (!IS_ENABLED(CONFIG_DATA_GNSS_BUFFER_STORE)) {
		return;
	}

	if (!new_gnss_data->queued) {
		return;
	}

	/* Go to start of buffer if end is reached. */
	head_gnss_buf += 1;
	if (head_gnss_buf == ARRAY_SIZE(gnss_buffer)) {
		head_gnss_buf = 0;
	}

	gnss_buffer[head_gnss_buf] = *new_gnss_data;

	LOG_DBG("Entry: %d of %ld in gnss buffer filled", head_gnss_buf,
		ARRAY_SIZE(gnss_buffer) - 1);
}

void cloud_codec_populate_modem_dynamic_buffer(struct cloud_data_modem_dynamic *new_modem_data)
{
	if (!IS_ENABLED(CONFIG_DATA_DYNAMIC_MODEM_BUFFER_STORE)) {
		return;
	}

	if (!new_modem_data->queued) {
		return;
	}

	/* Go to start of buffer if end is reached. */
	head_modem_dyn_buf += 1;
	if (head_modem_dyn_buf == ARRAY_SIZE(modem_dyn_buf)) {
		head_modem_dyn_buf = 0;
	}

	modem_dyn_buf[head_modem_dyn_buf] = *new_modem_data;

	LOG_DBG("Entry: %d of %ld in dynamic modem buffer filled",
		head_modem_dyn_buf, ARRAY_SIZE(modem_dyn_buf) - 1);
}

void cloud_codec_populate_modem_static_buffer(struct cloud_data_modem_static *new_modem_data)
{
	if (!new_modem_data->queued) {
		return;
	}

	/** Modem static buffer has only a single entry. */

	modem_stat = *new_modem_data;

	LOG_DBG("Static modem buffer filled");
}

void cloud_codec_populate_neighbor_cell_buffer(struct cloud_data_neighbor_cells *new_ncell_data)
{
	if (!new_ncell_data->queued) {
		return;
	}

	/* Neighbor cell buffer has only a single entry. */

	neighbor_cells = *new_ncell_data;

	LOG_DBG("Neighbor cell buffer filled");
}

void cloud_codec_retrieve_neighbor_cell_buffer(struct cloud_data_neighbor_cells *buffer)
{
	*buffer = neighbor_cells;
}

void cloud_codec_retrieve_modem_dynamic_buffer(struct cloud_data_modem_dynamic *buffer)
{
	*buffer = modem_dyn_buf[head_modem_dyn_buf];
}

void cloud_codec_retrieve_modem_static_buffer(struct cloud_data_modem_static *buffer)
{
	*buffer = modem_stat;
}

void cloud_codec_retrieve_gnss_buffer(struct cloud_data_gnss *buffer)
{
	*buffer = gnss_buffer[head_gnss_buf];
}

void cloud_codec_retrieve_ui_buffer(struct cloud_data_ui *buffer)
{
	*buffer = ui_buffer[head_ui_buf];
}

void cloud_codec_retrieve_accelerometer_buffer(struct cloud_data_accelerometer *buffer)
{
	*buffer = accel_buffer[head_accel_buf];
}

void cloud_codec_retrieve_sensors_buffer(struct cloud_data_sensors *buffer)
{
	*buffer = sensors_buffer[head_sensor_buf];
}

void cloud_codec_retrieve_battery_buffer(struct cloud_data_battery *buffer)
{
	*buffer = battery_buffer[head_bat_buf];
}
