/*
 * Copyright (c) 2024 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**
 * @file wifi_provision.h
 * @brief Header file for the Wi-Fi Provision Library.
 */

#ifndef WIFI_PROVISION_H__
#define WIFI_PROVISION_H__

/**
 * @defgroup wifi_provision Wi-Fi Provision library
 * @{
 * @brief Library used to provision a Wi-Fi device to a Wi-Fi network using HTTPS in softAP mode.
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Types of events generated during provisioning.
 */
enum wifi_provision_evt_type {
	/** The provisioning process has started. */
	WIFI_PROVISION_EVT_STARTED,

	/** A client has connected to the provisioning network. */
	WIFI_PROVISION_EVT_CLIENT_CONNECTED,

	/** A client has disconnected from the provisioning network. */
	WIFI_PROVISION_EVT_CLIENT_DISCONNECTED,

	/** Wi-Fi credentials received. */
	WIFI_PROVISION_EVT_CREDENTIALS_RECEIVED,

	/** The provisioning process has completed. */
	WIFI_PROVISION_EVT_COMPLETED,

	/** Wi-Fi credentials deleted, a reboot is required to enter a known unprovisioned state. */
	WIFI_PROVISION_EVT_RESET_REBOOT_REQUEST,

	/** The provisioning process has failed, irrecoverable error. */
	WIFI_PROVISION_EVT_FATAL_ERROR,
};

/**
 * @brief Structure containing event data from the provisioning process.
 */
struct wifi_provision_evt {
	/** Type of event. */
	enum wifi_provision_evt_type type;
};

/**
 * @brief Type for event handler callback function.
 *
 * @param[in] evt Event and associated parameters.
 */
typedef void (*wifi_provision_evt_handler_t)(const struct wifi_provision_evt *evt);

/**
 * @brief Initialize the Wi-Fi provisioning library.
 *
 * Must be called before any other function in the library.
 *
 * @param[in] handler Event handler for receiving asynchronous notifications.
 *
 * @retval -EINVAL if the handler is NULL.
 */
int wifi_provision_init(const wifi_provision_evt_handler_t handler);

/**
 * @brief Start the Wi-Fi provisioning process.
 *
 * Blocks until provisioning is completed.
 *
 * @returns 0 if successful. Otherwise, a negative error code is returned (errno.h).
 * @retval -ENOTSUP if the library is not initialized.
 * @retval -EINPROGRESS if provisioning is already in progress.
 */
int wifi_provision_start(void);

/**
 * @brief Reset the provisioning library.
 *
 * Deletes any stored Wi-Fi credentials and requests a reboot.
 *
 * @retval -ENOTSUP If the library is not initialized.
 */
int wifi_provision_reset(void);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* WIFI_PROVISION_H__ */
