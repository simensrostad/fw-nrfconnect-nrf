/*
 * Copyright (c) 2022 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-Nordic-5-Clause
 */

#include <zephyr.h>
#include <sys/slist.h>
#include <qos.h>

#include <logging/log.h>
LOG_MODULE_REGISTER(qos, CONFIG_QOS_LOG_LEVEL);

#define INVALID -1

/* Lookup table for QoS backoff timeouts. */
static uint32_t backoff_delay_lookup_sec[] = {
	16, 32, 64
};

/* Structure used to keep track of pending messages. Used in combination with a linked list. */
struct qos_metadata {
	/* Mandatory variable used to construct a linked list. */
	sys_snode_t header;

	/* Message associated with each node in the linked list. */
	struct qos_data message;
};

/* Structure containing internal variables in the library.  */
static struct ctx {
	/* Library event handler. Used in callbacks to the caller. */
	qos_evt_handler_t app_evt_handler;

	/* Number of times the message retransmission has been invoked by the library.
	 * Used to index backoff delay.
	 */
	uint8_t retry_count;

	/* Internal array of pending messages. Used in combination with linked list. */
	struct qos_metadata list_internal[CONFIG_QOS_PENDING_MESSAGES_MAX];

	/* Linked list used to keep track of pending messages. */
	sys_slist_t pending_list;

	/* Variable used to prevent multiple library initializations. */
	bool initialized;

	/* Delayed work used as the backoff timer. */
	struct k_work_delayable timeout_handler_work;
} ctx;

/* Mutex used to protect internal library variables. */
static K_MUTEX_DEFINE(ctx_lock);

/* Internal variable used to generate message IDs. */
static uint16_t message_id_next = QOS_MESSAGE_ID_BASE;

/* Internal API functions */

static void notify_event(const struct qos_evt *evt)
{
	__ASSERT(evt != NULL, "Library event not found");

	if (ctx.app_evt_handler != NULL) {
		ctx.app_evt_handler(evt);
	}
}

static int timeout_get(void)
{
	if (ctx.retry_count > ARRAY_SIZE(backoff_delay_lookup_sec) - 1) {
		struct qos_evt evt = {
			.type = QOS_EVT_RETRY_COUNT_EXPIRED
		};
		notify_event(&evt);

		/* Reset count after notification. */
		ctx.retry_count = 0;
		return -ERANGE;
	}

	return backoff_delay_lookup_sec[ctx.retry_count];
}

static int timer_start(void)
{
	if (!k_work_delayable_is_pending(&ctx.timeout_handler_work)) {
		int timeout = timeout_get();

		LOG_DBG("Timeout until next notification: %d", timeout);

		if (timeout < 0) {
			LOG_WRN("Maximum retries reached, abort message notification");
			return -ERANGE;
		}

		ctx.retry_count++;

		k_work_reschedule(&ctx.timeout_handler_work, K_SECONDS(timeout));
	}

	return 0;
}

static void timeout_handler_work_fn(struct k_work *work)
{
	int timeout = timeout_get();

	if (timeout < 0) {
		LOG_WRN("Maximum retries reached, abort message notification");
		return;
	}

	/* Notify all messages that are present in the internal pending list.
	 * Only if we get a valid timeout from timeout_get(). If timeout_get() returns a negative
	 * value, the event QOS_EVT_RETRY_COUNT_EXPIRED is notified that can trigger removal of
	 * all pending messages. There should not be messages in-flight in case that happens.
	 */
	qos_message_notify_all();

	ctx.retry_count++;

	k_work_reschedule(&ctx.timeout_handler_work, K_SECONDS(timeout));
}

/* @brief Function that appends a message to the internal list of pending messages.
 *
 * @returns A positive value indicating the index of the internal list that the message was
 *	    added to. Otherwise a negative value indicating the reason of failure.
 *
 * @retval -ENOMEM if the internal list is full.
 */
static int list_append(struct qos_data *message)
{
	int index_next = INVALID;

	/* Find the next available list index. */
	for (int i = 0; i < ARRAY_SIZE(ctx.list_internal) - 1; i++) {
		if (ctx.list_internal[i].message.notified_count == 0) {
			index_next = i;
			break;
		}
	}

	if (index_next == INVALID) {
		LOG_ERR("No available entries in pending message list");
		return -ENOMEM;
	}

	ctx.list_internal[index_next].message = *message;

	sys_slist_append(&ctx.pending_list, &ctx.list_internal[index_next].header);
	return index_next;
}

static int list_remove(uint32_t id)
{
	struct qos_metadata *node = NULL, *next_node = NULL, *node_prev = NULL;
	struct qos_evt evt = {
		.type = QOS_EVT_MESSAGE_REMOVED_FROM_LIST,
	};

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&ctx.pending_list, node, next_node, header) {
		if (node->message.id == id) {
			evt.message = node->message;
			notify_event(&evt);
			memset(&node->message, 0, sizeof(struct qos_data));
			sys_slist_remove(&ctx.pending_list, &node_prev->header, &node->header);
			return 0;
		}

		node_prev = node;
	};

	return -ENODATA;
}

/* Public API functions */

int qos_init(qos_evt_handler_t evt_handler)
{
	if (evt_handler == NULL) {
		LOG_ERR("Handler is NULL");
		return -EINVAL;
	}

	if (ctx.initialized) {
		return -EALREADY;
	}

	ctx.initialized = true;

	LOG_DBG("Registering handler %p", evt_handler);
	ctx.app_evt_handler = evt_handler;

	/* Initializing linked list and delayed work. */
	sys_slist_init(&ctx.pending_list);
	k_work_init_delayable(&ctx.timeout_handler_work, timeout_handler_work_fn);
	return 0;
}

int qos_message_add(struct qos_data *message)
{
	int err = 0, ret;
	struct qos_evt evt = {
		.type = QOS_EVT_MESSAGE_NEW,
		.message = *message
	};

	k_mutex_lock(&ctx_lock, K_FOREVER);

	if (message == NULL) {
		LOG_ERR("Message cannot be NULL");
		err = -EINVAL;
		goto exit;
	}

	/* Only ACK_REQUIRED messages are added to the internal list. */
	if (qos_message_has_flag(message, QOS_FLAG_RELIABILITY_ACK_REQUIRED)) {
		ret = list_append(message);
		if (ret < 0) {
			LOG_WRN("No list entries available, error: %d", ret);
			evt.type = QOS_EVT_MESSAGE_REMOVED_FROM_LIST;
			notify_event(&evt);
			err = -ENOMEM;
			goto exit;
		}

		ctx.list_internal[ret].message.notified_count++;
		notify_event(&evt);
	} else {
		notify_event(&evt);

		/* If the message does not carry the ACK_REQUIRED flag, its removed from the
		 * internal list immediately.
		 */
		evt.type = QOS_EVT_MESSAGE_REMOVED_FROM_LIST;
		notify_event(&evt);
	}

	timer_start();

exit:
	k_mutex_unlock(&ctx_lock);
	return err;
}

int qos_message_remove(uint32_t id)
{
	int err;

	k_mutex_lock(&ctx_lock, K_FOREVER);

	err = list_remove(id);
	if (err) {
		goto exit;
	}

	/* If the removed message is the last in the pending list, we stop the internal timer
	 * and clear the retry counter.
	 */
	if (sys_slist_is_empty(&ctx.pending_list)) {
		LOG_DBG("QoS list is empty!");
		ctx.retry_count = 0;
		k_work_cancel_delayable(&ctx.timeout_handler_work);
	}

exit:
	k_mutex_unlock(&ctx_lock);
	return err;
}

void qos_message_print(const struct qos_data *message)
{
	LOG_DBG("Notified count: %d", message->notified_count);
	LOG_DBG("Message heap_allocated: %d", message->heap_allocated);
	LOG_DBG("Message ID: %d", message->id);
	LOG_DBG("Message Buffer pointer: %p", message->data.buf);
	LOG_DBG("Message Buffer length: %d", message->data.len);
	LOG_DBG("Message Flags: %x", message->flags);
	LOG_DBG("Message type: %d", message->type);
}

bool qos_message_has_flag(const struct qos_data *message, uint32_t flag)
{
	return ((message->flags & flag) == flag) ? true : false;
}

uint16_t qos_message_id_get_next(void)
{
	if (message_id_next == UINT16_MAX) {
		message_id_next = QOS_MESSAGE_ID_BASE;
	}

	message_id_next++;

	return message_id_next;
}

void qos_message_notify_all(void)
{
	struct qos_metadata *node = NULL, *next_node = NULL;
	struct qos_evt evt = {
		.type = QOS_EVT_MESSAGE_TIMER_EXPIRED,
	};

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&ctx.pending_list, node, next_node, header) {
		node->message.notified_count++;
		evt.message = node->message;
		notify_event(&evt);
	};
}

void qos_message_remove_all(void)
{
	struct qos_metadata *node = NULL, *next_node = NULL;
	struct qos_evt evt = {
		.type = QOS_EVT_MESSAGE_REMOVED_FROM_LIST,
	};

	SYS_SLIST_FOR_EACH_CONTAINER_SAFE(&ctx.pending_list, node, next_node, header) {
		evt.message = node->message;
		notify_event(&evt);
		memset(&node->message, 0, sizeof(struct qos_data));
		sys_slist_remove(&ctx.pending_list, NULL, &node->header);
	};

	qos_timer_reset();
}

void qos_timer_reset(void)
{
	k_mutex_lock(&ctx_lock, K_FOREVER);

	k_work_cancel_delayable(&ctx.timeout_handler_work);
	ctx.retry_count = 0;

	k_mutex_unlock(&ctx_lock);
}
