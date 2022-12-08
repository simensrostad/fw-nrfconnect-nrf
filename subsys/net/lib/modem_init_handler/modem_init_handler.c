/*
 * Copyright 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr/sys/reboot.h>
#include <modem/nrf_modem_lib.h>
#include <net/lwm2m_client_utils.h>
#include <net/nrf_cloud.h>
#include <zephyr/logging/log.h>
#include <zephyr/logging/log_ctrl.h>

LOG_MODULE_REGISTER(modem_init_handler, CONFIG_MODEM_INIT_HANDLER_LOG_LEVEL);

/* Check the return code from nRF modem library initialization to ensure that
 * the modem is rebooted if a modem firmware update is ready to be applied.
 */
NRF_MODEM_LIB_ON_INIT(azure_fota_init_hook, on_modem_lib_init, NULL);

static void on_modem_lib_init(int ret, void *ctx)
{
	ARG_UNUSED(ctx);

	switch (ret) {
	case 0:
		/* Initialization successful, no action required. */
		return;
	case MODEM_DFU_RESULT_OK:
		LOG_DBG("MODEM UPDATE OK. Will run new modem firmware after reboot");
		break;
	case MODEM_DFU_RESULT_UUID_ERROR:
	case MODEM_DFU_RESULT_AUTH_ERROR:
		LOG_ERR("MODEM UPDATE ERROR %d. Will run old firmware", ret);
		break;
	case MODEM_DFU_RESULT_HARDWARE_ERROR:
	case MODEM_DFU_RESULT_INTERNAL_ERROR:
		LOG_ERR("MODEM UPDATE FATAL ERROR %d. Modem failure", ret);
		break;
	default:
		/* All non-zero return codes other than DFU result codes are
		 * considered irrecoverable and a reboot is needed.
		 */
		LOG_ERR("nRF modem lib initialization failed, error: %d", ret);
		break;
	}

/* Some FOTA libraries require that certain functions are called before a reboot can occur. */
#if defined(CONFIG_NRF_CLOUD_FOTA)
	/* Ignore return value, rebooting below */
	(void)nrf_cloud_fota_pending_job_validate(NULL);
#elif defined(LWM2M_CLIENT_UTILS_FIRMWARE_UPDATE_OBJ_SUPPORT)
	lwm2m_verify_modem_fw_update();
#endif

	LOG_DBG("Rebooting...");
	LOG_PANIC();
	sys_reboot(SYS_REBOOT_COLD);
}
