/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/kernel.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(CONFIG_NRF_MODEM_LIB)
#include <modem/lte_lc.h>
#include <modem/nrf_modem_lib.h>
#include <modem/modem_info.h>
#endif /* CONFIG_NRF_MODEM_LIB */
#include <net/aws_iot.h>
#include <zephyr/sys/reboot.h>
#include <date_time.h>
#include <zephyr/dfu/mcuboot.h>
#include <cJSON.h>
#include <cJSON_os.h>

BUILD_ASSERT(!IS_ENABLED(CONFIG_LTE_AUTO_INIT_AND_CONNECT), "Auto-init and connect not supported");

static struct k_work_delayable shadow_update_work;
static struct k_work_delayable aws_iot_connect_work;
static struct k_work shadow_update_version_work;

static bool cloud_connected, lte_connected;
static char modem_fw_version[50];

static K_SEM_DEFINE(lte_connected_sem, 0, 1);
static K_SEM_DEFINE(date_time_obtained, 0, 1);

/* Forward declarations */
static void date_time_event_handler(const struct date_time_evt *evt);
#if defined(CONFIG_LTE_LINK_CONTROL)
static void lte_handler(const struct lte_lc_evt *const evt);
#endif /* CONFIG_LTE_LINK_CONTROL */

static int json_add_obj(cJSON *parent, const char *str, cJSON *item)
{
	cJSON_AddItemToObject(parent, str, item);

	return 0;
}

static int json_add_str(cJSON *parent, const char *str, const char *item)
{
	cJSON *json_str;

	json_str = cJSON_CreateString(item);
	if (json_str == NULL) {
		return -ENOMEM;
	}

	return json_add_obj(parent, str, json_str);
}

static int json_add_number(cJSON *parent, const char *str, double item)
{
	cJSON *json_num;

	json_num = cJSON_CreateNumber(item);
	if (json_num == NULL) {
		return -ENOMEM;
	}

	return json_add_obj(parent, str, json_num);
}

static int shadow_update(bool version_number_include)
{
	int err, len = 0;
	char *message;
	int64_t message_ts = 0;
	int16_t bat_voltage = 0;

	err = date_time_now(&message_ts);
	if (err) {
		printk("date_time_now, error: %d\n", err);
		return err;
	}

#if defined(CONFIG_NRF_MODEM_LIB)
	/* Request battery voltage data from the modem. */
	len = modem_info_short_get(MODEM_INFO_BATTERY, &bat_voltage);
	if (len != sizeof(bat_voltage)) {
		printk("modem_info_short_get, error: %d\n", err);
		return -ENFILE;
	}
#endif

	cJSON *root_obj = cJSON_CreateObject();
	cJSON *state_obj = cJSON_CreateObject();
	cJSON *reported_obj = cJSON_CreateObject();

	if (root_obj == NULL || state_obj == NULL || reported_obj == NULL) {
		cJSON_Delete(root_obj);
		cJSON_Delete(state_obj);
		cJSON_Delete(reported_obj);
		return -ENOMEM;
	}

	if (version_number_include) {
		err += json_add_str(reported_obj, "app_version", CONFIG_APP_VERSION);
		err += json_add_str(reported_obj, "modem_version", modem_fw_version);
	}

	err += json_add_number(reported_obj, "batv", bat_voltage);
	err += json_add_number(reported_obj, "ts", (double)message_ts);
	err += json_add_obj(state_obj, "reported", reported_obj);
	err += json_add_obj(root_obj, "state", state_obj);

	if (err) {
		printk("json_add, error: %d\n", err);
		goto cleanup;
	}

	message = cJSON_Print(root_obj);
	if (message == NULL) {
		printk("cJSON_Print, error: returned NULL\n");
		err = -ENOMEM;
		goto cleanup;
	}

	struct aws_iot_data tx_data = {
		.qos = MQTT_QOS_0_AT_MOST_ONCE,
		.topic.type = AWS_IOT_SHADOW_TOPIC_UPDATE,
		.ptr = message,
		.len = strlen(message)
	};

	printk("Publishing: %s to AWS IoT broker\n", message);

	err = aws_iot_send(&tx_data);
	if (err) {
		printk("aws_iot_send, error: %d\n", err);
	}

	cJSON_FreeString(message);

cleanup:

	cJSON_Delete(root_obj);

	return err;
}

static void aws_iot_connect_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);

	int err;

	if (!lte_connected) {
		printk("LTE is not connected! Abort cloud connection attempt\n");
		goto schedule_connection_attempt;
	}

	if (cloud_connected) {
		return;
	}

	err = aws_iot_connect(NULL);
	if (err) {
		printk("aws_iot_connect, error: %d\n", err);
	}

schedule_connection_attempt:
	printk("Next connection retry in %d seconds\n", CONFIG_CONNECTION_RETRY_TIMEOUT_SECONDS);
	(void)k_work_schedule(&aws_iot_connect_work, K_SECONDS(CONFIG_CONNECTION_RETRY_TIMEOUT_SECONDS));
}

static void shadow_update_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);

	int err;

	if (!cloud_connected) {
		return;
	}

	err = shadow_update(false);
	if (err) {
		printk("shadow_update, error: %d\n", err);
	}

	printk("Next data publication in %d seconds\n", CONFIG_PUBLICATION_INTERVAL_SECONDS);
	(void)k_work_schedule(&shadow_update_work, K_SECONDS(CONFIG_PUBLICATION_INTERVAL_SECONDS));
}

static void shadow_update_version_work_fn(struct k_work *work)
{
	ARG_UNUSED(work);

	int err;

	err = shadow_update(true);
	if (err) {
		printk("shadow_update, error: %d\n", err);
	}
}

static void print_received_data(const char *buf, const char *topic, size_t topic_len)
{
	char *str = NULL;
	cJSON *root_obj = NULL;

	root_obj = cJSON_Parse(buf);
	if (root_obj == NULL) {
		printk("cJSON Parse failure");
		return;
	}

	str = cJSON_Print(root_obj);
	if (str == NULL) {
		printk("Failed to print JSON object");
		goto clean_exit;
	}

	printf("Data received from AWS IoT console:\nTopic: %.*s\nMessage: %s\n",
	       topic_len, topic, str);

	cJSON_FreeString(str);

clean_exit:
	cJSON_Delete(root_obj);
}

/* Connect to LTE and AWS IoT */
static void connect(void)
{
	/* Cancel ongoing connect work, if any. */
	(void)k_work_cancel_delayable(&aws_iot_connect_work);

#if defined(CONFIG_NRF_MODEM_LIB)
	(void)modem_info_string_get(MODEM_INFO_FW_VERSION,
				    modem_fw_version,
				    sizeof(modem_fw_version));

	printk("Current modem firwmare version: %s\n", modem_fw_version);

	int err = lte_lc_init_and_connect_async(lte_handler);

	if (err) {
		printk("Failed to configure the modem, error: %d\n", err);
		return;
	}

	k_sem_take(&lte_connected_sem, K_FOREVER);
#endif /* CONFIG_NRF_MODEM_LIB */

	/* Trigger a date time update. The date_time API is used to timestamp data that is sent
	 * to AWS IoT.
	 */
	date_time_update_async(date_time_event_handler);

	/* Postpone connecting to AWS IoT until date time has been obtained. */
	k_sem_take(&date_time_obtained, K_FOREVER);
	(void)k_work_schedule(&aws_iot_connect_work, K_NO_WAIT);
}

/* Function that reinitializes the nRF Modem library and connects to LTE and AWS IoT. */
#if defined(CONFIG_NRF_MODEM_LIB)
static void reinit_modem_and_connect(void)
{
	int err;

	/* Gracefully disconnect from AWS IoT before reinitializing the modem. */
	(void)aws_iot_disconnect();

	/* Deinitialize Link controller to put modem in offline mode.
	 * This is a requirement for reinitializing the modem.
	 */
	(void)lte_lc_deinit();

	/* Shutdown and initialize the nRF Modem library to perform the update.
	 * Expect that nrf_modem_lib_init returns MODEM_DFU_RESULT_OK which signifies that DFU
	 * completed successfully and a reboot (reinitialization) is needed.
	 */
	err = nrf_modem_lib_shutdown();
	if (err) {
		printk("Failed shutting down the modem\n");
		return;
	}

	err = nrf_modem_lib_init(NORMAL_MODE);
	if ((err < 0) || ((err > 0) && (err != MODEM_DFU_RESULT_OK))) {
		printk("Initializing the modem failed (perform update), error: %d\n", err);
		return;
	}

	/* Shutdown and initialize the nRF Modem library again to run the new firmware.
	 * Expect that nrf_modem_lib_init returns 0 which signifies that the modem has been
	 * successfully initialized and the new firmware is running.
	 */
	err = nrf_modem_lib_shutdown();
	if (err) {
		printk("Failed shutting down the modem\n");
		return;
	}

	err = nrf_modem_lib_init(NORMAL_MODE);
	if ((err < 0) || ((err > 0) && (err != MODEM_DFU_RESULT_OK))) {
		printk("Initializing the modem failed (run the new image), error: %d\n", err);
		return;
	}

	printk("Modem reinitialized");

	connect();
}
#endif /* CONFIG_NRF_MODEM_LIB */

void aws_iot_event_handler(const struct aws_iot_evt *const evt)
{
	switch (evt->type) {
	case AWS_IOT_EVT_CONNECTING:
		printk("AWS_IOT_EVT_CONNECTING\n");
		break;
	case AWS_IOT_EVT_CONNECTED:
		printk("AWS_IOT_EVT_CONNECTED\n");

		cloud_connected = true;

		/* This may fail if the work item is already being processed,
		 * but in such case, the next time the work handler is executed,
		 * it will exit after checking the above flag and the work will
		 * not be scheduled again.
		 */
		(void)k_work_cancel_delayable(&aws_iot_connect_work);

		if (evt->data.persistent_session) {
			printk("Persistent session enabled\n");
		}

#if defined(CONFIG_NRF_MODEM_LIB)
		/** Successfully connected to AWS IoT broker, mark image as
		 *  working to avoid reverting to the former image upon reboot.
		 */
		boot_write_img_confirmed();
#endif

		/** Send version number to AWS IoT broker to verify that the
		 *  FOTA update worked.
		 */
		(void)k_work_submit(&shadow_update_version_work);

		/** Start sequential shadow data updates.
		 */
		(void)k_work_schedule(&shadow_update_work,
				      K_SECONDS(CONFIG_PUBLICATION_INTERVAL_SECONDS));

#if defined(CONFIG_NRF_MODEM_LIB)
		int err = lte_lc_psm_req(true);

		if (err) {
			printk("Requesting PSM failed, error: %d\n", err);
		}
#endif
		break;
	case AWS_IOT_EVT_READY:
		printk("AWS_IOT_EVT_READY\n");
		break;
	case AWS_IOT_EVT_DISCONNECTED:
		printk("AWS_IOT_EVT_DISCONNECTED\n");

		cloud_connected = false;

		/* This may fail if the work item is already being processed,
		 * but in such case, the next time the work handler is executed,
		 * it will exit after checking the above flag and the work will
		 * not be scheduled again.
		 */
		(void)k_work_cancel_delayable(&shadow_update_work);

		/* Attempt to reconnect in 5 seconds. */
		(void)k_work_schedule(&aws_iot_connect_work, K_SECONDS(5));
		break;
	case AWS_IOT_EVT_DATA_RECEIVED:
		printk("AWS_IOT_EVT_DATA_RECEIVED\n");

		print_received_data(evt->data.msg.ptr,
				    evt->data.msg.topic.str,
				    evt->data.msg.topic.len);

		break;
	case AWS_IOT_EVT_PUBACK:
		printk("AWS_IOT_EVT_PUBACK, message ID: %d\n", evt->data.message_id);
		break;
	case AWS_IOT_EVT_FOTA_START:
		printk("AWS_IOT_EVT_FOTA_START\n");
		break;
	case AWS_IOT_EVT_FOTA_ERASE_PENDING:
		printk("AWS_IOT_EVT_FOTA_ERASE_PENDING\n");
		break;
	case AWS_IOT_EVT_FOTA_ERASE_DONE:
		printk("AWS_FOTA_EVT_ERASE_DONE\n");
		break;
	case AWS_IOT_EVT_FOTA_APPLICATION_DONE:
		printk("AWS_IOT_EVT_FOTA_APPLICATION_DONE\n");
		printk("Application image update done, rebooting device\n");

		(void)aws_iot_disconnect();
		(void)sys_reboot(0);

		break;
	case AWS_IOT_EVT_FOTA_MODEM_DELTA_DONE:
		printk("AWS_IOT_EVT_FOTA_MODEM_DELTA_DONE\n");
		printk("Modem delta image update done\n");

		reinit_modem_and_connect();

		break;
	case AWS_IOT_EVT_FOTA_DL_PROGRESS:
		printk("AWS_IOT_EVT_FOTA_DL_PROGRESS, (%d%%)\n", evt->data.fota_progress);
		break;
	case AWS_IOT_EVT_ERROR:
		printk("AWS_IOT_EVT_ERROR, %d\n", evt->data.err);
		break;
	case AWS_IOT_EVT_FOTA_ERROR:
		printk("AWS_IOT_EVT_FOTA_ERROR");
		break;
	default:
		printk("Unknown AWS IoT event type: %d\n", evt->type);
		break;
	}
}

static void work_init(void)
{
	(void)k_work_init_delayable(&shadow_update_work, shadow_update_work_fn);
	(void)k_work_init_delayable(&aws_iot_connect_work, aws_iot_connect_work_fn);
	(void)k_work_init(&shadow_update_version_work, shadow_update_version_work_fn);
}

#if defined(CONFIG_NRF_MODEM_LIB)
static void lte_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_NW_REG_STATUS:
		if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
		     (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING)) {
			lte_connected = false;
			break;
		}

		printk("Network registration status: %s\n",
			evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ?
			"Connected - home network" : "Connected - roaming");

		lte_connected = true;
		k_sem_give(&lte_connected_sem);

		break;
	case LTE_LC_EVT_PSM_UPDATE:
		printk("PSM parameter update: TAU: %d, Active time: %d\n",
			evt->psm_cfg.tau, evt->psm_cfg.active_time);
		break;
	case LTE_LC_EVT_EDRX_UPDATE: {
		char log_buf[60];
		ssize_t len;

		len = snprintf(log_buf, sizeof(log_buf),
			       "eDRX parameter update: eDRX: %f, PTW: %f",
			       evt->edrx_cfg.edrx, evt->edrx_cfg.ptw);
		if (len > 0) {
			printk("%s\n", log_buf);
		}
	}

		break;
	case LTE_LC_EVT_RRC_UPDATE:
		printk("RRC mode: %s\n",
			evt->rrc_mode == LTE_LC_RRC_MODE_CONNECTED ?
			"Connected" : "Idle");
		break;
	case LTE_LC_EVT_CELL_UPDATE:
		printk("LTE cell changed: Cell ID: %d, Tracking area: %d\n",
			evt->cell.id, evt->cell.tac);

		if (evt->cell.id == -1 && evt->cell.tac == -1) {
			lte_connected = false;
		}

		break;
	default:
		printk("Unknown event: %d\n", evt->type);
		break;
	}
}
#endif /* CONFIG_NRF_MODEM_LIB */

static int app_topics_subscribe(void)
{
	int err;
	static char custom_topic[75] = "my-custom-topic/example";
	static char custom_topic_2[75] = "my-custom-topic/example_2";

	const struct aws_iot_topic_data topics_list[CONFIG_AWS_IOT_APP_SUBSCRIPTION_LIST_COUNT] = {
		[0].str = custom_topic,
		[0].len = strlen(custom_topic),
		[1].str = custom_topic_2,
		[1].len = strlen(custom_topic_2)
	};

	err = aws_iot_subscription_topics_add(topics_list,
					      ARRAY_SIZE(topics_list));
	if (err) {
		printk("aws_iot_subscription_topics_add, error: %d\n", err);
	}

	return err;
}

static void date_time_event_handler(const struct date_time_evt *evt)
{
	switch (evt->type) {
	case DATE_TIME_OBTAINED_MODEM:
		/* Fall through */
	case DATE_TIME_OBTAINED_NTP:
		/* Fall through */
	case DATE_TIME_OBTAINED_EXT:
		printk("Date time obtained\n");
		k_sem_give(&date_time_obtained);

		/* De-register handler. At this point the sample will have
		 * date time to depend on indefinitely until a reboot occurs.
		 */
		date_time_register_handler(NULL);
		break;
	case DATE_TIME_NOT_OBTAINED:
		printk("DATE_TIME_NOT_OBTAINED\n");
		break;
	default:
		printk("Unknown event: %d", evt->type);
		break;
	}
}

void main(void)
{
	int err;

	printk("The AWS IoT sample started, version: %s\n", CONFIG_APP_VERSION);

	cJSON_Init();
	work_init();

	err = aws_iot_init(NULL, aws_iot_event_handler);
	if (err) {
		printk("AWS IoT library could not be initialized, error: %d\n", err);
	}

	/** Subscribe to customizable non-shadow specific topics
	 *  to AWS IoT backend.
	 */
	err = app_topics_subscribe();
	if (err) {
		printk("Adding application specific topics failed, error: %d\n", err);
		return;
	}

	connect();
}
