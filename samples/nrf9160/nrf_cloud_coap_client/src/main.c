/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <stdio.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/net/coap.h>
#include <zephyr/net/socket.h>
#include <modem/lte_lc.h>
#include <modem/modem_info.h>
#include <modem/nrf_modem_lib.h>
#include <zephyr/random/rand32.h>
#include <dk_buttons_and_leds.h>
#include <nrf_socket.h>
#include <nrf_modem_at.h>
#include <date_time.h>
#include <net/nrf_cloud.h>
#include <net/nrf_cloud_coap.h>
#include <version.h>
#include "scan_wifi.h"
#include "handle_fota.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(nrf_cloud_coap_client, CONFIG_NRF_CLOUD_COAP_CLIENT_LOG_LEVEL);

#define CREDS_REQ_WAIT_SEC 10
#define APP_WAIT_CELLS_S 30
#define BTN_NUM 1
#define APP_COAP_SEND_INTERVAL_MS 20000
#define APP_COAP_INTERVAL_LIMIT 60

/* Uncomment to incrementally increase time between coap packets */
/* #define DELAY_INTERPACKET_PERIOD */

/* Modem FW version required to properly run this sample */
#define MFWV_MAJ_SAMPLE_REQ	1
#define MFWV_MIN_SAMPLE_REQ	3
#define MFWV_REV_SAMPLE_REQ	0
/* Modem FW version required for extended neighbor cells search */
#define MFWV_MAJ_EXT_SRCH	1
#define MFWV_MIN_EXT_SRCH	3
#define MFWV_REV_EXT_SRCH	1
/* Modem FW version required for extended GCI neighbor cells search */
#define MFWV_MAJ_EXT_SRCH_GCI	1
#define MFWV_MIN_EXT_SRCH_GCI	3
#define MFWV_REV_EXT_SRCH_GCI	4

/* Type of data to be sent in the cellular positioning request */
enum nrf_cloud_location_type active_cell_pos_type = LOCATION_TYPE_SINGLE_CELL;

static bool connected;

/* Search type used for neighbor cell measurements; modem FW version depenedent */
static enum lte_lc_neighbor_search_type search_type =
						  LTE_LC_NEIGHBOR_SEARCH_TYPE_GCI_EXTENDED_COMPLETE;

/* Buffer to hold neighbor cell measurement data for multi-cell requests */
static struct lte_lc_ncell neighbor_cells[CONFIG_LTE_NEIGHBOR_CELLS_MAX];

/* Buffer to hold GCI cell measurement data for multi-cell requests */
static struct lte_lc_cell gci_cells[5];

/* Modem info struct used for modem FW version and cell info used for single-cell requests */
#if defined(CONFIG_MODEM_INFO)
static struct modem_param_info mdm_param;
#endif

/* Structure to hold all cell info */
static struct lte_lc_cells_info cell_info;

/* Current RRC mode */
static enum lte_lc_rrc_mode cur_rrc_mode = LTE_LC_RRC_MODE_IDLE;

/* Flag to indicate that a neighbor cell measurement should be taken once RRC mode is idle */
static bool request_cells = true;

static bool pgps_active;

/* Semaphore to indicate that cell info has been received */
static K_SEM_DEFINE(cell_info_ready_sem, 0, 1);

/* Mutex for cell info struct */
static K_MUTEX_DEFINE(cell_info_mutex);

#if defined(CONFIG_WIFI)
/* Semaphore to indicate Wi-Fi scanning is complete */
static K_SEM_DEFINE(wifi_scan_sem, 0, 1);
#endif

static void get_cell_info(void);
#if defined(CONFIG_NRF_CLOUD_PGPS)
static int do_pgps(struct gps_pgps_request *pgps_req);
#endif
static int update_shadow(void);

static bool ver_check(int32_t reqd_maj, int32_t reqd_min, int32_t reqd_rev,
		      int32_t maj, int32_t min, int32_t rev)
{
	if (maj > reqd_maj) {
		return true;
	} else if ((maj == reqd_maj) && (min > reqd_min)) {
		return true;
	} else if ((maj == reqd_maj) && (min == reqd_min) && (rev >= reqd_rev)) {
		return true;
	}
	return false;
}

static void check_modem_fw_version(void)
{
	char mfwv_str[128];
	uint32_t major;
	uint32_t minor;
	uint32_t rev;

#if defined(CONFIG_MODEM_INFO)
	if (modem_info_string_get(MODEM_INFO_FW_VERSION, mfwv_str, sizeof(mfwv_str)) <= 0) {
		LOG_WRN("Failed to get modem FW version");
		return;
	}
#else
	strncpy(mfwv_str, "1.3.0", sizeof(mfwv_str));
#endif
	LOG_INF("Modem FW version: %s", mfwv_str);

	if (sscanf(mfwv_str, "mfw_nrf9160_%u.%u.%u", &major, &minor, &rev) != 3) {
		LOG_WRN("Unable to parse modem FW version number");
		return;
	}

	/* Ensure the modem firmware version meets the requirement for this sample */
	if (!ver_check(MFWV_MAJ_SAMPLE_REQ, MFWV_MIN_SAMPLE_REQ, MFWV_REV_SAMPLE_REQ,
		       major, minor, rev)) {
		LOG_ERR("This sample requires modem FW version %d.%d.%d or later",
			MFWV_MAJ_SAMPLE_REQ, MFWV_MIN_SAMPLE_REQ, MFWV_REV_SAMPLE_REQ);
		LOG_INF("Update modem firmware and restart");
		k_sleep(K_FOREVER);
	}

	/* Enable GCI/extended search if modem fw version allows */
	if (ver_check(MFWV_MAJ_EXT_SRCH_GCI, MFWV_MIN_EXT_SRCH_GCI, MFWV_REV_EXT_SRCH_GCI,
		      major, minor, rev)) {
		search_type = LTE_LC_NEIGHBOR_SEARCH_TYPE_GCI_EXTENDED_COMPLETE;
		LOG_INF("Using LTE LC neighbor search type GCI extended complete for %d cells",
			ARRAY_SIZE(gci_cells));
	} else if (ver_check(MFWV_MAJ_EXT_SRCH, MFWV_MIN_EXT_SRCH, MFWV_REV_EXT_SRCH,
			     major, minor, rev)) {
		search_type = LTE_LC_NEIGHBOR_SEARCH_TYPE_EXTENDED_COMPLETE;
		LOG_INF("Using LTE LC neighbor search type extended complete");
	} else {
		LOG_INF("Using LTE LC neighbor search type default");
	}
}

#if defined(CONFIG_NRF_MODEM_LIB)
/**@brief Recoverable modem library error. */
void nrf_modem_recoverable_error_handler(uint32_t err)
{
	LOG_ERR("Modem library recoverable error: %u", (unsigned int)err);
}
#endif /* defined(CONFIG_NRF_MODEM_LIB) */

K_SEM_DEFINE(lte_ready, 0, 1);

static void lte_handler(const struct lte_lc_evt *const evt)
{
	switch (evt->type) {
	case LTE_LC_EVT_NW_REG_STATUS:
		if ((evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_HOME) ||
		    (evt->nw_reg_status == LTE_LC_NW_REG_REGISTERED_ROAMING)) {
			LOG_DBG("Connected to LTE network");
			k_sem_give(&lte_ready);
		} else {
			LOG_DBG("reg status %d", evt->nw_reg_status);
		}
		break;

	case LTE_LC_EVT_CELL_UPDATE:
		if (evt->cell.id == LTE_LC_CELL_EUTRAN_ID_INVALID) {
			break;
		}

		/* Get new info when cell ID changes */
		LOG_DBG("Cell info changed");
		request_cells = true;
		get_cell_info();
		break;
	case LTE_LC_EVT_RRC_UPDATE:
		cur_rrc_mode = evt->rrc_mode;
		if (cur_rrc_mode == LTE_LC_RRC_MODE_IDLE) {
			LOG_DBG("RRC mode: idle");
		} else {
			LOG_DBG("RRC mode: connected");
		}
		if (request_cells && (cur_rrc_mode == LTE_LC_RRC_MODE_IDLE)) {
			get_cell_info();
		}
		break;
	case LTE_LC_EVT_NEIGHBOR_CELL_MEAS:
		if ((search_type < LTE_LC_NEIGHBOR_SEARCH_TYPE_GCI_DEFAULT) &&
		    (evt->cells_info.current_cell.id == LTE_LC_CELL_EUTRAN_ID_INVALID)) {
			LOG_WRN("Current cell ID not valid in neighbor cell measurement results");
			break;
		}

		(void)k_mutex_lock(&cell_info_mutex, K_FOREVER);
		/* Copy current cell information. */
		memcpy(&cell_info.current_cell,
		       &evt->cells_info.current_cell,
		       sizeof(cell_info.current_cell));

		/* Copy neighbor cell information if present. */
		cell_info.ncells_count = evt->cells_info.ncells_count;
		if ((evt->cells_info.ncells_count > 0) && (evt->cells_info.neighbor_cells)) {
			memcpy(neighbor_cells,
			       evt->cells_info.neighbor_cells,
			       sizeof(neighbor_cells[0]) * cell_info.ncells_count);
			LOG_DBG("Received measurements for %u neighbor cells",
				cell_info.ncells_count);
		} else {
			LOG_DBG("No neighbor cells were measured");
		}

		/* Copy GCI cell information if present. */
		cell_info.gci_cells_count = evt->cells_info.gci_cells_count;
		if ((evt->cells_info.gci_cells_count > 0) && (evt->cells_info.gci_cells)) {
			memcpy(gci_cells,
			       evt->cells_info.gci_cells,
			       sizeof(gci_cells[0]) * cell_info.gci_cells_count);
			LOG_DBG("Received measurements for %u GCI cells",
				cell_info.gci_cells_count);
		} else if (search_type == LTE_LC_NEIGHBOR_SEARCH_TYPE_GCI_EXTENDED_COMPLETE) {
			LOG_DBG("No GCI cells were measured");
		}

		(void)k_mutex_unlock(&cell_info_mutex);
		k_sem_give(&cell_info_ready_sem);

		break;
	default:
		LOG_DBG("LTE event %d (0x%x)", evt->type, evt->type);
		break;
	}
}

#if defined(CONFIG_NRF_CLOUD_PGPS)
void pgps_handler(struct nrf_cloud_pgps_event *event)
{
	switch (event->type) {
	case PGPS_EVT_INIT:
		LOG_INF("PGPS_EVT_INIT");
		break;
	case PGPS_EVT_UNAVAILABLE:
		LOG_INF("PGPS_EVT_UNAVAILABLE");
		break;
	case PGPS_EVT_LOADING:
		LOG_INF("PGPS_EVT_LOADING");
		break;
	case PGPS_EVT_AVAILABLE:
		LOG_INF("PGPS_EVT_AVAILABLE");
		break;
	case PGPS_EVT_READY:
		LOG_INF("PGPS_EVT_READY");
		pgps_active = false;
		break;
	case PGPS_EVT_REQUEST:
		LOG_INF("PGPS_EVT_REQUEST");
		pgps_active = true;
		do_pgps(event->request);
		break;
	}
}
#endif

/**@brief Configures modem to provide LTE link. Blocks until link is
 * successfully established.
 */
static void modem_configure(void)
{
	int err;

	err = nrf_modem_lib_init();
	if (err < 0) {
		LOG_ERR("Modem library initialization failed, error: %d", err);
		return;
	} else if (err > 0) {
		LOG_INF("Modem firmware update in progress: 0x%X", err);
		return;
	}

	lte_lc_register_handler(lte_handler);
#if defined(CONFIG_LTE_LINK_CONTROL)
	LOG_INF("LTE Link Connecting ...");
	err = lte_lc_init_and_connect();
	__ASSERT(err == 0, "LTE link could not be established.");
	k_sem_take(&lte_ready, K_FOREVER);
	LOG_INF("LTE Link Connected");
	err = lte_lc_psm_req(true);
	if (err) {
		LOG_ERR("Unable to enter PSM mode: %d", err);
	}

	err = nrf_modem_at_printf("AT+CEREG=5");
	if (err) {
		LOG_ERR("Can't subscribe to +CEREG events.");
	}
#endif /* defined(CONFIG_LTE_LINK_CONTROL) */
	/* Modem info library is used to obtain the modem FW version
	 * and network info for single-cell requests
	 */
#if defined(CONFIG_MODEM_INFO)
	err = modem_info_init();
	if (err) {
		LOG_ERR("Modem info initialization failed, error: %d", err);
		return;
	}

	err = modem_info_params_init(&mdm_param);
	if (err) {
		LOG_ERR("Modem info params initialization failed, error: %d", err);
		return;
	}

	err = modem_info_params_get(&mdm_param);
	if (err) {
		LOG_ERR("Modem info params reading failed, error: %d", err);
	}
#endif

	/* Check modem FW version */
	check_modem_fw_version();
}

int init(void)
{
	int err;

	err = handle_fota_init();
	if (err) {
		LOG_ERR("Error initializing FOTA: %d", err);
	}

	modem_configure();

	err = handle_fota_begin();
	if (err) {
		return err;
	}

#if defined(CONFIG_WIFI)
	err = scan_wifi_init();
	if (err) {
		LOG_ERR("Error initializing Wi-Fi scanning: %d", err);
		return err;
	}
#endif

	err = nrf_cloud_coap_init();
	if (err) {
		LOG_ERR("Failed to initialize CoAP client: %d", err);
		return err;
	}

	err = nrf_cloud_coap_connect();
	if (err) {
		LOG_ERR("Failed to connect and get authorized: %d", err);
		return err;
	}
	connected = true;
	get_cell_info();

	err = update_shadow();
	if (err) {
		LOG_ERR("Error updating shadow");
		return err;
	}
	LOG_INF("Shadow updated");

#if defined(CONFIG_NRF_CLOUD_PGPS)
	struct nrf_cloud_pgps_init_param param = {
		.event_handler = pgps_handler,
		.storage_base = 0,
		.storage_size = 0
	};

	err = nrf_cloud_pgps_init(&param);
	if (err) {
		LOG_ERR("Failed to initialize P-GPS: %d", err);
	}
#endif
	return err;
}

static void get_cell_info(void)
{
	int err;

	if (!connected || !request_cells) {
		return;
	}

	struct lte_lc_ncellmeas_params ncellmeas_params = {
		.search_type = search_type,
		.gci_count = ARRAY_SIZE(gci_cells)
	};

	/* Set the result buffers */
	cell_info.neighbor_cells = neighbor_cells;
	cell_info.gci_cells = gci_cells;

	LOG_INF("Requesting neighbor cell measurement");
	err = lte_lc_neighbor_cell_measurement(&ncellmeas_params);
	if (err) {
		LOG_ERR("Failed to start neighbor cell measurement, error: %d", err);
	} else {
		request_cells = false;
		LOG_INF("Waiting for measurement results...");
	}
}

#if defined(CONFIG_NRF_CLOUD_PGPS)
static int do_pgps(struct gps_pgps_request *pgps_req)
{
	int err;
	struct nrf_cloud_rest_pgps_request pgps_request;
	struct nrf_cloud_pgps_result pgps_res;
	static char host[64];
	static char path[128];

	LOG_INF("******** Getting P-GPS data");
	memset(&pgps_request, 0, sizeof(pgps_request));
	memset(&pgps_res, 0, sizeof(pgps_res));
	pgps_request.pgps_req = pgps_req;
	LOG_INF("period_min:%u, pred_cnt:%u, gps_day:%u, gps_time:%u",
		pgps_req->prediction_period_min, pgps_req->prediction_count,
		pgps_req->gps_day, pgps_req->gps_time_of_day);
	memset(host, 0, sizeof(host));
	memset(path, 0, sizeof(path));
	pgps_res.host = host;
	pgps_res.host_sz = sizeof(host);
	pgps_res.path = path;
	pgps_res.path_sz = sizeof(path);
	err = nrf_cloud_coap_pgps_url_get(&pgps_request, &pgps_res);
	if (err) {
		LOG_ERR("Failed to request P-GPS: %d", err);
		return err;
	}
	LOG_INF("P-GPS host:%s, host_sz:%u, path:%s, path_sz:%u",
		pgps_res.host, pgps_res.host_sz, pgps_res.path, pgps_res.path_sz);
	err = nrf_cloud_pgps_update(&pgps_res);
	if (err) {
		nrf_cloud_pgps_request_reset();
		LOG_ERR("P-GPS data processing failed, error: %d", err);
		return err;
	}

	LOG_DBG("P-GPS data processed");
	err = nrf_cloud_pgps_notify_prediction();
	if (err) {
		LOG_ERR("GNSS: Failed to request current prediction, error: %d", err);
	} else {
		LOG_DBG("P-GPS prediction requested");
	}
	return err;
}
#endif

#if defined(CONFIG_NRF_CLOUD_AGPS)
static uint8_t agps_buf[4096];

static int do_agps(void)
{
	int err;
	static bool got_agps;
	struct nrf_cloud_rest_agps_request agps_request;
	struct nrf_modem_gnss_agps_data_frame agps_req;
	struct nrf_cloud_rest_agps_result agps_res;

	if (got_agps) {
		LOG_INF("Not required yet.");
		return 0;
	}
	memset(&agps_request, 0, sizeof(agps_request));
	memset(&agps_req, 0, sizeof(agps_req));
	agps_request.type = NRF_CLOUD_REST_AGPS_REQ_ASSISTANCE;
	agps_request.net_info = &cell_info;
	agps_request.agps_req = &agps_req;
	agps_req.data_flags = 0x3f;
	agps_req.sv_mask_alm = 0xffffffff;
	agps_req.sv_mask_ephe = 0xffffffff;
	agps_res.buf = agps_buf;
	agps_res.buf_sz = sizeof(agps_buf);
	err = nrf_cloud_coap_agps_data_get(&agps_request, &agps_res);
	if (err) {
		LOG_ERR("Failed to request A-GPS: %d", err);
	} else {
		LOG_INF("Processing %u bytes of A-GPS data", agps_res.agps_sz);
		err = nrf_cloud_agps_process(agps_res.buf, agps_res.agps_sz);
		if (err) {
			LOG_ERR("A-GPS data processing failed, error: %d", err);
		}  else {
			LOG_INF("A-GPS data processed");
			got_agps = true;
		}
	}
	return err;
}
#endif

static int update_shadow(void)
{
	/* Enable FOTA for bootloader, modem and application */
	struct nrf_cloud_svc_info_fota fota = {
		.modem = 1,
		.application = 1
	};
	struct nrf_cloud_svc_info_ui ui_info = {
		.gnss = true,
		.temperature = true
	};
	struct nrf_cloud_svc_info service_info = {
		.fota = &fota,
		.ui = &ui_info
	};
	struct nrf_cloud_modem_info modem_info = {
		.device = NRF_CLOUD_INFO_SET,
		.network = NRF_CLOUD_INFO_SET,
		.sim = IS_ENABLED(CONFIG_MODEM_INFO_ADD_SIM) ? NRF_CLOUD_INFO_SET : 0,
		/* Use the modem info already obtained */
#if defined(CONFIG_MODEM_INFO)
		.mpi = &mdm_param,
#endif
		/* Include the application version */
		.application_version = CONFIG_NRF_CLOUD_COAP_CLIENT_SAMPLE_VERSION
	};
	struct nrf_cloud_device_status device_status = {
		.modem = &modem_info,
		.svc = &service_info
	};

	return nrf_cloud_coap_shadow_device_status_update(&device_status);
}

static int do_next_test(void)
{
	static double temp = 21.5;
	static int cur_test = 1;
	static struct nrf_cloud_gnss_data gnss;
	static const char * const location_types[] = {
		"SINGLE_CELL",
		"MULTI_CELL",
		"WIFI"
	};
	int err = 0;
	struct nrf_cloud_rest_location_request loc_request;
	struct nrf_cloud_location_result result;
	struct wifi_scan_info *wifi_info = NULL;
	char buf[512];
	const char *type;

	if (!gnss.type) {
		gnss.type = NRF_CLOUD_GNSS_TYPE_PVT;
		gnss.pvt.lat = 45.525616;
		gnss.pvt.lon = -122.685978;
		gnss.pvt.accuracy = 30;
	}

	printk("\n***********************************************\n");
	switch (cur_test) {
	case 1:
		printk("**** %d. Getting pending FOTA job execution ****\n", cur_test);
		err = handle_fota_process();
		if (err != -EAGAIN) {
			LOG_INF("FOTA check completed.");
		}
		break;
	case 2:
		printk("*** %d. Sending temperature ********************\n", cur_test);
		err = nrf_cloud_coap_sensor_send(NRF_CLOUD_JSON_APPID_VAL_TEMP, temp);
		if (err) {
			LOG_ERR("Error sending sensor data: %d", err);
			break;
		}
		LOG_INF("Sent %.1f C", temp);
		temp += 0.1;
		break;
	case 3:
		printk("*** %d. Getting position ***********************\n", cur_test);
		LOG_INF("Waiting for neighbor cells..");
		err = k_sem_take(&cell_info_ready_sem, K_SECONDS(APP_WAIT_CELLS_S));
		if (err) {
			LOG_ERR("Timeout waiting for cells: %d", err);
			break;
		}
#if defined(CONFIG_WIFI)
		err = scan_wifi_start(&wifi_scan_sem);
		LOG_INF("Waiting for Wi-Fi scans...");
		k_sem_take(&wifi_scan_sem, K_FOREVER);
		if (err) {
			LOG_ERR("Error starting Wi-Fi scan: %d", err);
			break;
		}
		wifi_info = scan_wifi_results_get();
#endif

		(void)k_mutex_lock(&cell_info_mutex, K_FOREVER);

		if (cell_info.current_cell.id != LTE_LC_CELL_EUTRAN_ID_INVALID) {
			LOG_INF("Current cell info: Cell ID: %u, TAC: %u, MCC: %d, MNC: %d",
				cell_info.current_cell.id, cell_info.current_cell.tac,
				cell_info.current_cell.mcc, cell_info.current_cell.mnc);
		} else {
			LOG_WRN("No current serving cell available");
		}

		if (cell_info.ncells_count || cell_info.gci_cells_count) {
			LOG_INF("Performing multi-cell request with "
				"%u neighbor cells and %u GCI cells",
				cell_info.ncells_count, cell_info.gci_cells_count);
		} else {
			LOG_INF("Performing single-cell request");
		}

		loc_request.cell_info = &cell_info;
		loc_request.wifi_info = wifi_info;
		err = nrf_cloud_coap_location_get(&loc_request, &result);
		(void)k_mutex_unlock(&cell_info_mutex);
		if (err) {
			LOG_ERR("Unable to get location: %d", err);
			break;
		}

		if ((result.type >= 0) && (result.type < LOCATION_TYPE__INVALID)) {
			type = location_types[result.type];
		} else {
			type = "unknown";
		}
		/* Process the returned location once it arrives */
		gnss.pvt.lat = result.lat;
		gnss.pvt.lon = result.lon;
		gnss.pvt.accuracy = result.unc;
		LOG_INF("Location: %s, %.12g, %.12g, %d", type,
			result.lat, result.lon, result.unc);

		request_cells = true;
		break;
	case 4:
		printk("*** %d. Sending GNSS PVT ***********************\n", cur_test);
		err = nrf_cloud_coap_location_send(&gnss);
		if (err) {
			LOG_ERR("Error sending GNSS PVT data: %d", err);
		} else {
			LOG_INF("PVT sent");
		}
		break;
	case 5:
		printk("*** %d. Getting A-GPS data *********************\n", cur_test);
#if defined(CONFIG_NRF_CLOUD_AGPS)
		err = do_agps();
#endif
		break;
	case 6:
		printk("*** %d. Getting shadow delta *******************\n", cur_test);
		buf[0] = '\0';
		err = nrf_cloud_coap_shadow_get(buf, sizeof(buf) - 1, true);
		if (err) {
			LOG_ERR("Failed to request shadow delta: %d", err);
		} else {
			size_t len = strlen(buf);

			LOG_INF("Delta: %s", len ? buf : "None");
			if (len) {
				err = nrf_cloud_coap_shadow_state_update(buf);
				if (err) {
					LOG_ERR("Failed to acknowledge delta: %d", err);
				} else {
					LOG_INF("Delta acknowledged");
				}
			}
		}
		break;
	}

	if (++cur_test > 6) {
		cur_test = 1;
	}
	return err;
}

int main(void)
{
	int64_t next_msg_time;
	int delta_ms = APP_COAP_SEND_INTERVAL_MS;
	int err;
	int i = 1;

	LOG_INF("\n");
	LOG_INF("The nRF Cloud CoAP client sample started\n");

	err = init();
	if (err) {
		LOG_ERR("Halting.");
		for (;;) {
		}
	}
	next_msg_time = k_uptime_get() + delta_ms;

	while (1) {
		if (!pgps_active && (k_uptime_get() >= next_msg_time)) {

			if (!connected) {
				LOG_INF("Going online");
				err = lte_lc_normal();
				if (err) {
					LOG_ERR("Error going online: %d", err);
				} else {
					k_sem_take(&lte_ready, K_FOREVER);
					err = nrf_cloud_coap_connect();
					if (err) {
						LOG_ERR("Failed to connect and get authorized: %d",
							err);
						break;
					}
					connected = true;
				}
			}

			if (connected) {
				err = do_next_test();

				if (err == -EAGAIN) {
					connected = false;
					err = nrf_cloud_coap_disconnect();
					if (err) {
						LOG_ERR("Error closing socket: %d", err);
					} else {
						LOG_INF("Socket closed.");
					}
					LOG_INF("Going offline");
					err = lte_lc_offline();
					if (err) {
						LOG_ERR("Error going offline: %d", err);
					} else {
						LOG_INF("Offline.");
					}
				}
			}

			delta_ms = APP_COAP_SEND_INTERVAL_MS * i;
			next_msg_time += delta_ms;
#if defined(DELAY_INTERPACKET_PERIOD)
			LOG_INF("Next transfer in %d minutes, %d seconds",
				delta_ms / 60000, (delta_ms / 1000) % 60);
			if (++i > APP_COAP_INTERVAL_LIMIT) {
				i = APP_COAP_INTERVAL_LIMIT;
			}
#endif
		}
		k_sleep(K_MSEC(100));
	}
	nrf_cloud_coap_disconnect();
	return err;
}
