/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <modem/at_cmd.h>
#include <modem/lte_lc.h>

#include <logging/log.h>
#include <logging/log_ctrl.h>

LOG_MODULE_REGISTER(xcountrydata, CONFIG_XCOUNTRYDATA_LOG_LEVEL);

K_SEM_DEFINE(lte_connected, 0, 1);

/* 1. entry:
 *	- act: LTE-M
 *	- mcc: 242      - Norway
 *	|- band: 20     |
 *	|- EARFCN 6175  |
 *      ---------------
 *	|- band: 13     |
 *	|- EARFCN: 0    |
 * 2. entry:
 *	- act: NB-IoT
 *	- mcc: 244      - Finland
 *	|- band: 3      |
 *	|- EARFCN: 1207 |
 */
#define COUNTRY_DATA_SET "AT%XCOUNTRYDATA=1, \"4,242,20,6175,13,0\", \"5,244,3,1207\""
#define COUNTRY_DATA_DELETE "AT%XCOUNTRYDATA=0"
#define COUNTRY_DATA_READ "AT%XCOUNTRYDATA?"

static void lte_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_NW_REG_STATUS:
		if ((evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_HOME) &&
		     (evt->nw_reg_status != LTE_LC_NW_REG_REGISTERED_ROAMING)) {
			break;
		}

		printk("Network registration status: %s\n",
			evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME ?
			"Connected - home network" : "Connected - roaming\n");
		k_sem_give(&lte_connected);
		break;
	default:
		break;
	}
}

void main(void)
{
	int err;
	char response[60];

	LOG_INF("XCOUNTRYDATA sample started");

	/* Initialize the link controller */
	err = lte_lc_init();
	if (err) {
		LOG_ERR("Failed to initialize LTE link controller, error: %d", err);
		return;
	}

	/* Set country specific search configuration before activating LTE modem. */
	err = at_cmd_write(COUNTRY_DATA_SET, NULL, 0, NULL);
	if (err) {
		LOG_ERR("Failed to set countrydata");
		return;
	}

	LOG_INF("Country data configuration set");

	/* Connect to LTE */
	err = lte_lc_connect_async(lte_handler);
	if (err) {
		LOG_ERR("Failed to connect to LTE network, error: %d\n", err);
		return;
	}

	k_sem_take(&lte_connected, K_FOREVER);

	LOG_INF("LTE link established");

	/* Read and print country data */
	err = at_cmd_write(COUNTRY_DATA_READ, response, sizeof(response), NULL);
	if (err) {
		LOG_ERR("Failed to read country data");
		return;
	}

	LOG_INF("Country data read out");

	LOG_INF("Response: %s", log_strdup(response));

	/* Turn off modem before deleting country data */
	lte_lc_power_off();

	LOG_INF("Modem turned off");

	/* Delete country data from the modem */
	err = at_cmd_write(COUNTRY_DATA_DELETE, NULL, 0, NULL);
	if (err) {
		LOG_ERR("Failed to delete countrydata");
		return;
	}

	LOG_INF("Country data deleted");
}
