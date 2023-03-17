/*
 * Copyright (c) 2023 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#ifndef NRF_CLOUD_COAP_H_
#define NRF_CLOUD_COAP_H_

/** @file nrf_cloud_coap.h
 * @brief Module to provide nRF Cloud CoAP API
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <net/nrf_cloud_rest.h>
#include <net/nrf_cloud_agps.h>
#include <net/nrf_cloud_pgps.h>
#include <zephyr/net/coap_client.h>

/**
 * @defgroup nrf_cloud_coap nRF CoAP API
 * @{
 */

/* Transport functions */
/**@brief Initialize nRF Cloud CoAP library.
 *
 * @return 0 if initialization was successful, otherwise, a negative error number.
 */
int nrf_cloud_coap_init(void);

/**@brief Connect to and obtain authorization to access the nRF Cloud CoAP server.
 *
 * Upon successful authorization, call @ref nrf_cloud_coap_is_connected to confirm
 * whether the device was granted access to nRF Cloud.
 *
 * @return 0 if authorized successfully, otherwise, a negative error number.
 */
int nrf_cloud_coap_connect(void);

/**@brief Check if device is connected and authorized to use nRF Cloud CoAP.
 *
 * A device is authorized if the JWT it POSTed to the /auth/jwt endpoint is valid
 * and matches credentials for a device already provisioned and associated with nRF Cloud.
 *
 * @retval true Device is allowed to access nRF Cloud services.
 * @retval false Device is disallowed from accessing nRF Cloud services.
 */
bool nrf_cloud_coap_is_connected(void);

/**@brief Perform CoAP GET request.
 *
 * The function will block until the response or an error have been returned.
 *
 * @param resource String containing the specific CoAP endpoint to access.
 * @param query Optional string containing REST-style query parameters.
 * @param buf Optional pointer to buffer containing a payload to include with the request.
 * @param len Length of payload or 0 if none.
 * @param fmt_out CoAP content format for the Content-Format message option of the payload.
 * @param fmt_in CoAP content format for the Accept message option of the returned payload.
 * @param reliable True to use a Confirmable message, otherwise, a Non-confirmable message.
 * @param cb Pointer to a callback function to receive the results.
 * @param user Pointer to user-specific data to be passed back to the callback.
 * @return 0 if the request succeeded, a positive value indicating a CoAP result code,
 * or a negative error number.
 */
int nrf_cloud_coap_get(const char *resource, const char *query,
		       uint8_t *buf, size_t len,
		       enum coap_content_format fmt_out,
		       enum coap_content_format fmt_in, bool reliable,
		       coap_client_response_cb_t cb, void *user);

/**@brief Perform CoAP POST request.
 *
 * The function will block until the response or an error have been returned. Use this
 * function to send custom JSON or CBOR messages to nRF Cloud through the
 * https://api.nrfcloud.com/v1#tag/Messages/operation/SendDeviceMessage API.
 *
 * @param resource String containing the specific CoAP endpoint to access.
 * @param query Optional string containing REST-style query parameters.
 * @param buf Optional pointer to buffer containing a payload to include with the request.
 * @param len Length of payload or 0 if none.
 * @param fmt CoAP content format for the Content-Format message option of the payload.
 * @param reliable True to use a Confirmable message, otherwise, a Non-confirmable message.
 * @param cb Pointer to a callback function to receive the results.
 * @param user Pointer to user-specific data to be passed back to the callback.
 * @return 0 if the request succeeded, a positive value indicating a CoAP result code,
 * or a negative error number.
 */
int nrf_cloud_coap_post(const char *resource, const char *query,
			uint8_t *buf, size_t len,
			enum coap_content_format fmt, bool reliable,
			coap_client_response_cb_t cb, void *user);

/**@brief Perform CoAP PUT request.
 *
 * The function will block until the response or an error have been returned.
 *
 * @param resource String containing the specific CoAP endpoint to access.
 * @param query Optional string containing REST-style query parameters.
 * @param buf Optional pointer to buffer containing a payload to include with the request.
 * @param len Length of payload or 0 if none.
 * @param fmt CoAP content format for the Content-Format message option of the payload.
 * @param reliable True to use a Confirmable message, otherwise, a Non-confirmable message.
 * @param cb Pointer to a callback function to receive the results.
 * @param user Pointer to user-specific data to be passed back to the callback.
 * @return 0 if the request succeeded, a positive value indicating a CoAP result code,
 * or a negative error number.
 */
int nrf_cloud_coap_put(const char *resource, const char *query,
		       uint8_t *buf, size_t len,
		       enum coap_content_format fmt, bool reliable,
		       coap_client_response_cb_t cb, void *user);

/**@brief Perform CoAP DELETE request.
 *
 * The function will block until the response or an error have been returned.
 *
 * @param resource String containing the specific CoAP endpoint to access.
 * @param query Optional string containing REST-style query parameters.
 * @param buf Optional pointer to buffer containing a payload to include with the request.
 * @param len Length of payload or 0 if none.
 * @param fmt CoAP content format for the Content-Format message option of the payload.
 * @param reliable True to use a Confirmable message, otherwise, a Non-confirmable message.
 * @param cb Pointer to a callback function to receive the results.
 * @param user Pointer to user-specific data to be passed back to the callback.
 * @return 0 if the request succeeded, a positive value indicating a CoAP result code,
 * or a negative error number.
 */
int nrf_cloud_coap_delete(const char *resource, const char *query,
			  uint8_t *buf, size_t len,
			  enum coap_content_format fmt, bool reliable,
			  coap_client_response_cb_t cb, void *user);

/**@brief Perform CoAP FETCH request.
 *
 * The function will block until the response or an error have been returned.
 *
 * @param resource String containing the specific CoAP endpoint to access.
 * @param query Optional string containing REST-style query parameters.
 * @param buf Optional pointer to buffer containing a payload to include with the request.
 * @param len Length of payload or 0 if none.
 * @param fmt_out CoAP content format for the Content-Format message option of the payload.
 * @param fmt_in CoAP content format for the Accept message option of the returned payload.
 * @param reliable True to use a Confirmable message, otherwise, a Non-confirmable message.
 * @param cb Pointer to a callback function to receive the results.
 * @param user Pointer to user-specific data to be passed back to the callback.
 * @return 0 if the request succeeded, a positive value indicating a CoAP result code,
 * or a negative error number.
 */
int nrf_cloud_coap_fetch(const char *resource, const char *query,
			 uint8_t *buf, size_t len,
			 enum coap_content_format fmt_out,
			 enum coap_content_format fmt_in, bool reliable,
			 coap_client_response_cb_t cb, void *user);

/**@brief Perform CoAP PATCH request.
 *
 * The function will block until the response or an error have been returned.
 *
 * @param resource String containing the specific CoAP endpoint to access.
 * @param query Optional string containing REST-style query parameters.
 * @param buf Optional pointer to buffer containing a payload to include with the GET request.
 * @param len Length of payload or 0 if none.
 * @param fmt CoAP content format for the Content-Format message option of the payload.
 * @param reliable True to use a Confirmable message, otherwise, a Non-confirmable message.
 * @param cb Pointer to a callback function to receive the results.
 * @param user Pointer to user-specific data to be passed back to the callback.
 * @return 0 if the request succeeded, a positive value indicating a CoAP result code,
 * or a negative error number.
 */
int nrf_cloud_coap_patch(const char *resource, const char *query,
			 uint8_t *buf, size_t len,
			 enum coap_content_format fmt, bool reliable,
			 coap_client_response_cb_t cb, void *user);

/**@brief Disconnect the nRF Cloud CoAP connection
 *
 * This does not teardown the thread in coap_client, as there is no way to do so.
 * The thread's call to poll(sock) will fail, resulting in an error message.
 * This is expected. Call nrf_cloud_coap_connect() to re-establish the connection, and
 * the thread in coap_client will resume.
 *
 * @return 0 if the socket was closed successfully, or a negative error number.
 */
int nrf_cloud_coap_disconnect(void);

/* nRF Cloud service functions */

/** @brief nRF Cloud CoAP Assisted GPS (A-GPS) data request.
 *
 * @param[in]     request Data to be provided in API call.
 * @param[in,out] result Structure pointing to caller-provided buffer in which to store A-GPS data.
 *
 * @retval 0 If successful.
 *           Otherwise, a (negative) error code is returned:
 *           - -EINVAL will be returned, and an error message printed, if invalid parameters
 *              are given.
 *           - -ENOENT will be returned if there was no A-GPS data requested for the specified
 *              request type.
 *           - -ENOBUFS will be returned, and an error message printed, if there is not enough
 *             buffer space to store retrieved AGPS data.
 */
int nrf_cloud_coap_agps_data_get(struct nrf_cloud_rest_agps_request const *const request,
				 struct nrf_cloud_rest_agps_result *result);

/** @brief nRF Cloud Predicted GPS (P-GPS) request URL.
 *
 *  After a successful call to this function, pass the file_location to
 *  nrf_cloud_pgps_update(), which then downloads and processes the file's binary P-GPS data.
 *
 * @param[in]     request       Data to be provided in API call.
 * @param[in,out] file_location Structure that will contain the host and path to
 *                              the prediction file.
 *
 * @retval 0 If successful.
 *          Otherwise, a (negative) error code is returned.
 */
int nrf_cloud_coap_pgps_url_get(struct nrf_cloud_rest_pgps_request const *const request,
				 struct nrf_cloud_pgps_result *file_location);

/** @brief Send a sensor value to nRF Cloud.
 *
 *  The CoAP message is sent as a non-confirmable CoAP message.
 *
 * @param[in]     app_id The app_id identifying the type of data. See the values in nrf_cloud_defs.h
 *                       that begin with  NRF_CLOUD_JSON_APPID_. You may also use custom names.
 * @param[in]     value  Sensor reading.
 *
 * @retval 0 If successful.
 *          Otherwise, a (negative) error code is returned.
 */
int nrf_cloud_coap_sensor_send(const char *app_id, double value);

/** @brief Send the device location in the :ref:`nrf_cloud_gnss_data` PVT field to nRF Cloud.
 *
 *  The CoAP message is sent as a non-confirmable CoAP message. Only
 *  :ref:`NRF_CLOUD_GNSS_TYPE_PVT` is supported.
 *
 * @param[in]     gnss A pointer to an :ref:`nrf_cloud_gnss_data` struct indicating the device
 *                     location, usually as determined by the GNSS unit.
 *
 * @retval 0 If successful.
 *          Otherwise, a (negative) error code is returned.
 */
int nrf_cloud_coap_location_send(const struct nrf_cloud_gnss_data * const gnss);

/**
 * @brief nRF Cloud location request.
 *
 * At least one of cell_info or wifi_info must be provided within the request.
 *
 * @param[in]     request Data to be provided in API call.
 * @param[in,out] result Location information.
 *
 * @return 0 if the request succeeded, a positive value indicating a CoAP result code,
 * or a negative error number.
 */
int nrf_cloud_coap_location_get(struct nrf_cloud_rest_location_request const *const request,
				struct nrf_cloud_location_result *const result);

/**
 * @brief Requests current nRF Cloud FOTA job info for the specified device.
 *
 * @param[out]    job Parsed job info. If no job exists, type will
 *                    be set to invalid. If a job exists, user must call
 *                    @ref nrf_cloud_rest_fota_job_free to free the memory
 *                    allocated by this function.
 *
 * @return 0 if the request succeeded, a positive value indicating a CoAP result code,
 * or a negative error number.
 */
int nrf_cloud_coap_fota_job_get(struct nrf_cloud_fota_job_info *const job);

/**
 * @brief Frees memory allocated by nrf_cloud_coap_current_fota_job_get().
 *
 * @param[in,out] job Job info to be freed.
 *
 */
void nrf_cloud_coap_fota_job_free(struct nrf_cloud_fota_job_info *const job);

/**
 * @brief Updates the status of the specified nRF Cloud FOTA job.
 *
 * @param[in]     job_id Null-terminated FOTA job identifier.
 * @param[in]     status Status of the FOTA job.
 * @param[in]     details Null-terminated string containing details of the
 *                job, such as an error description.
 *
 * @return 0 if the request succeeded, a positive value indicating a CoAP result code,
 * or a negative error number.
 */
int nrf_cloud_coap_fota_job_update(const char *const job_id,
	const enum nrf_cloud_fota_status status, const char * const details);

/**
 * @brief Queries the device's shadow delta.
 *
 * @param[in,out] buf     Pointer to memory in which to receive the delta.
 * @param[in]     buf_len Size of buffer.
 * @param[in]     delta   True to request only changes in the shadow, if any; otherwise,
 *                        all of desired part.
 *
 * @return 0 if the request succeeded, a positive value indicating a CoAP result code,
 * or a negative error number.
 */
int nrf_cloud_coap_shadow_get(char *buf, size_t buf_len, bool delta);

/**
 * @brief Updates the device's "state" in the shadow via the UpdateDeviceState endpoint.
 *
 * @param[in]     shadow_json Null-terminated JSON string to be written to the device's shadow.
 *
 * @return 0 if the request succeeded, a positive value indicating a CoAP result code,
 * or a negative error number.
 */
int nrf_cloud_coap_shadow_state_update(const char * const shadow_json);

/**
 * @brief Update the device status in the shadow.
 *
 * @param[in]     dev_status Device status to be encoded.
 *
 * @return 0 if the request succeeded, a positive value indicating a CoAP result code,
 * or a negative error number.
 */
int nrf_cloud_coap_shadow_device_status_update(const struct nrf_cloud_device_status
					       *const dev_status);

/**
 * @brief Updates the device's "ServiceInfo" in the shadow.
 *
 * @param[in]     svc_inf Service info items to be updated in the shadow.
 *
 * @return 0 if the request succeeded, a positive value indicating a CoAP result code,
 * or a negative error number.
 */
int nrf_cloud_coap_shadow_service_info_update(const struct nrf_cloud_svc_info * const svc_inf);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* NRF_CLOUD_COAP_H_ */
