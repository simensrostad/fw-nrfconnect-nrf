/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**@file
 *
 * @brief   QoS library
 */

#ifndef QOS_H__
#define QOS_H__

#include <zephyr.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @defgroup qos_flag_bitmask QoS library bitmask values
 *
 * @brief Use these bitmask values to enable different QoS message flags.
 *
 * @details The values can be OR'ed together to enable multiple flags at the same time. If a
 *          systems bit is 0, the corresponding system is disabled.
 * @{
 */

/** @brief Set this flag to disable acknowleding of the message. */
#define QOS_FLAG_RELIABILITY_ACK_DISABLED 0x01;

/**
 * @brief Set this flag to require acknowledging of the message.
 *
 * @details By setting this flag the caller will be notified periodically with the
 * 	    QOS_EVT_MESSAGE_TIMER_EXPIRED event until qos_message_remove() has been called with
 *	    the corresponding message. The periodic backoff schemas is configures via the
 *	    CONFIG_QOS_PERIODIC_BACKOFF_PERIOD Kconfig option.
 */
#define QOS_FLAG_RELIABILITY_ACK_REQUIRED 0x02;

/**
 * @brief Set this flag for low priority messages.
 *
 * @details Low priority messages should be
 */
#define QOS_FLAG_PRIORITY_LOW 0x03;

/**
 * @brief
 *
 * @details
 */
#define QOS_FLAG_PRIORITY_NORMAL 0x04;

/**
 * @brief
 *
 * @details
 */
#define QOS_FLAG_PRIORITY_HIGH 0x05;

/**
 * @brief
 *
 * @details
 */
#define QOS_FLAG_PRIORITY_ALRAM 0x06;

/** @} */

enum qos_evt_type {

	/** A new message is ready. */
	QOS_EVT_MESSAGE_NEW,

	/** Retransmission timer has expired for a message.
	 *  Payload is of type  @ref qos_data.
	 *
	 *  comment: The structure contains the data type so that it can be addressed to the
	 *           correct endpoint.
	 *
	 *  comment: We could even have one event, QOS_EVT_MESSAGE_READY instead of both
	 *	     QOS_EVT_MESSAGE_TIMER_EXPIRED and QOS_EVT_MESSAGE_NEW
	 *
	 */
	QOS_EVT_MESSAGE_TIMER_EXPIRED,

	/** Event received when the internal list is full or message has been removed
	 *  from the internal list using qos_message_remove. If the heap_allocated flag is
	 *  set in the payload the corresponding buffer must be freed.
	 *  Payload is of type  @ref qos_data.
	 */
	QOS_EVT_MESSAGE_REMOVED_FROM_LIST
};

/** Enum used to identify the type of data in the message.
 *
 *  Comment: Could be possible to use xmacro here to generate the list run-time to avoid
 *           application specific hard-coded types.
 */
enum qos_data_type {
	UNUSED,
	/** Genric data type. */
	GENERIC,
	/** Data type used for batched messages. */
	BATCH,
	/** Data type used for UI messages. */
	UI,
	/** Data type used for neighbor cell measurements. */
	NEIGHBOR_CELLS,
	/** Data type used for AGPS requests. */
	AGPS_REQUEST,
	/** Data type used for configuration updates. */
	CONFIG,
};

/** Structure that specifies the data to be sent with corresponding metadata. */
struct qos_data {
	/** Data. */
	uint8_t *buf;
	/** Length of data. */
	size_t len;
	/** Flags associated with the message.
	 *  see @ref qos_flag_bitmask for documentation on the various flags that can be set.
	*/
	uint32_t qos_flags;
	/** Type of data to be sent. */
	enum qos_data_type type;
	/** Flag signifying if the data has been allocated by the caller. */
	bool heap_allocated;
};

struct qos_evt {
	enum qos_evt_type type;
	struct qos_data message;
};

/** @brief QoS library event handler.
 *
 *  @param[in] evt The event and any associated parameters.
 */
typedef void (*qos_evt_handler_t)(const struct qos_evt *evt);

/** @brief
 *
 *  @param
 */
int qos_init(qos_evt_handler_t evt_handler);

/** @brief Add message to internal list of messages to be sent.
 *
 *  @param message Pointer to message
 *
 *  @retval -EINVAL
 *  @retval -EBADMSG
 *  @retval -EFAULT
 */
int qos_message_add(const struct qos_data *message);

/** @brief Remove message from internal list. If the list item has the memory allocated
 *         flag set, an event QOS_EVT_MESSAGE_REMOVED_FROM_LIST will be sent out that contains
 *         the data item that must be freed.
 *
 *  @param message Pointer to message
 *
 *  @retval -EINVAL
 *  @retval -EBADMSG
 *  @retval -EFAULT
 */
int qos_message_remove(const struct qos_data *message);

#ifdef __cplusplus
}
#endif

#endif /* QOS_H__ */
