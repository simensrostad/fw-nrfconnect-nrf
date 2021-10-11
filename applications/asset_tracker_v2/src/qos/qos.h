/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

/**@file
 *
 * @brief   QoS library for Asset Tracker v2
 */

#ifndef QOS_H__
#define QOS_H__

#include <zephyr.h>

#ifdef __cplusplus
extern "C" {
#endif

enum qos_evt_type {
	/** A new message is ready. */
	QOS_EVT_MESSAGE_NEW,
	/** Retransmission timer has expired for a message.
	 *  Payload is of type  @ref qos_data.
	 *
	 *  The structure contains the data type so that it can be addressed to the
	 *  correct endpoint.
	 *
	 */
	QOS_EVT_MESSAGE_ACK_TIMER_EXPIRED,
	/** Message has been ACKed. If the memory_allocated flag is set in the payload the
	 *  corresponding buffer must be freed.
	 *  Payload is of type  @ref qos_data
	 */
	QOS_EVT_MESSAGE_ACKED,

	/** Event received when the internal list is full or message has been explicitly removed
	 *  from the internal list. If the memory_allocated flag is set in the payload the
	 *  corresponding buffer must be freed.
	 *  Payload is of type  @ref qos_data.
	 */
	QOS_EVT_MESSAGE_REMOVED_FROM_LIST
};

/* Enum signifying the realiabilty level of the message. */
enum qos_reliability {
	/** This option requires that the message is not ACKed. */
	QOS_RELIABILITY_NO_ACK,
	/** This option requires that the message should be ACKed. By specifying this option
	 *  the message will be retransmitted with a backoff in case its not ACKed.
	 */
	QOS_RELIABILITY_ACK_REQUIRED,
};

// /** Enum used to specify the priority of the message. Higher priority message must be sent
//  *  regardless. High priority messages will be
//  *
//  * comment: Setting a high priority of a message means that it will be sent by the cloud module
//  *          no matter what the current lte connection
//  *
//  * I wonder if this option is nessecary. If its up to the cloud module to send messages based on
//  * the type of the message.
//  *
//  */
// enum qos_energy_consumption_min {
// 	/** If this option is set the message has lower priority */
// 	QOS_PRIORITY_LOW,
// 	/** If this option is set the message has higher priority */
// 	QOS_PRIORITY_HIGH,
// };

/** Enum used to identify the type of data in the message. */
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
	/** Reliability of the message. */
	enum qos_reliability reliability;
	// /** Priority of the message. */
	// enum qos_priority priority;
	/** Type of data to be sent, used to address data to correct endpoint. */
	enum qos_data_type type;
	/** Flag signifying if the data has been allocated.
	 *
	 * comment: I'm not sure if this flag is needed. The fact that we handle pointer to
	 * the buffer implicitly states that data has been allocated somewhere. Maybe rename it
	 * to needs_free or something more instructive?
	 *
	 */
	bool memory_allocated;
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
 */
int qos_message_add(const struct qos_data *message);

/** @brief A message has been ACKed. If the message has the memory_allocated flag set an event
 *        QOS_EVT_MESSAGE_ACKED will be sent out that contains the data to be freed.
 *
 *  @param message Pointer to message
 */
int qos_message_acked(const struct qos_data *message);

/** @brief Explicity remove message from internal list. If the list item has the memory allocated
 *         flag set, an event QOS_EVT_MESSAGE_REMOVED_FROM_LIST will be sent out that contains
 *         the data item that must be freed.
 *
 *  @param message Pointer to message
 */
int qos_message_remove(const struct qos_data *message);

#ifdef __cplusplus
}
#endif

#endif /* QOS_H__ */

/* qos_message_add and qos_message_acked basically does the same thing. We could remove the
 * concept of ACK and cloud related concepts and make it as generic as possible.
 */
