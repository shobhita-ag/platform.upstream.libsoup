/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-message-queue.c: Message queue
 *
 * Copyright (C) 2003, Ximian, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "soup-message-queue.h"

struct SoupMessageQueue {
	GList *head, *tail;
	GList *iters;
};

/**
 * soup_message_queue_new:
 *
 * Return value: a new #SoupMessageQueue object
 **/
SoupMessageQueue *
soup_message_queue_new (void)
{
	return g_new0 (SoupMessageQueue, 1);
}

/**
 * soup_message_queue_destroy:
 * @queue: a message queue
 *
 * Frees memory associated with @queue, which must be empty.
 **/
void
soup_message_queue_destroy (SoupMessageQueue *queue)
{
	g_return_if_fail (queue->head == NULL);

	g_list_free (queue->head);
	g_list_free (queue->iters);
	g_free (queue);
}

/**
 * soup_message_queue_append:
 * @queue: a queue
 * @msg: a message
 *
 * Appends @msg to the end of @queue
 **/
void
soup_message_queue_append (SoupMessageQueue *queue, SoupMessage *msg)
{
	if (queue->head) {
		queue->tail = g_list_append (queue->tail, msg);
		queue->tail = queue->tail->next;
	} else
		queue->head = queue->tail = g_list_append (NULL, msg);

	g_object_add_weak_pointer (G_OBJECT (msg), &queue->tail->data);
}

/**
 * soup_message_queue_first:
 * @queue: a queue
 * @iter: pointer to a #SoupMessageQueueIter
 *
 * Initializes @iter and returns the first element of @queue. If you
 * do not iterate all the way to the end of the list, you must call
 * soup_message_queue_free_iter() to dispose the iterator when you are
 * done.
 *
 * Return value: the first element of @queue, or %NULL if it is empty.
 **/
SoupMessage *
soup_message_queue_first (SoupMessageQueue *queue, SoupMessageQueueIter *iter)
{
	if (!queue->head)
		return NULL;

	queue->iters = g_list_prepend (queue->iters, iter);

	iter->cur = NULL;
	iter->next = queue->head;
	return soup_message_queue_next (queue, iter);
}

/**
 * soup_message_queue_next:
 * @queue: a queue
 * @iter: pointer to an initialized #SoupMessageQueueIter
 *
 * Return value: the next element of @queue, or %NULL if there are no more.
 **/
SoupMessage *
soup_message_queue_next (SoupMessageQueue *queue, SoupMessageQueueIter *iter)
{
	while (iter->next) {
		iter->cur = iter->next;
		iter->next = iter->cur->next;
		if (iter->cur->data)
			return iter->cur->data;

		/* Message was finalized, remove dead queue element */
		soup_message_queue_remove (queue, iter);
	}

	/* Nothing left */
	iter->cur = NULL;
	soup_message_queue_free_iter (queue, iter);
	return NULL;
}

/**
 * soup_message_queue_remove:
 * @queue: a queue
 * @iter: pointer to an initialized #SoupMessageQueueIter
 *
 * Removes the queue element pointed to by @iter; that is, the last
 * message returned by soup_message_queue_first() or
 * soup_message_queue_next().
 *
 * Return value: the removed message, or %NULL if the element pointed
 * to by @iter was already removed.
 **/
SoupMessage *
soup_message_queue_remove (SoupMessageQueue *queue, SoupMessageQueueIter *iter)
{
	GList *i;
	SoupMessageQueueIter *iter2;
	SoupMessage *msg;

	if (!iter->cur) {
		/* We're at end of list or this item was already removed */
		return NULL;
	}

	/* Fix any other iters pointing to iter->cur */
	for (i = queue->iters; i; i = i->next) {
		iter2 = i->data;
		if (iter2 != iter) {
			if (iter2->cur == iter->cur)
				iter2->cur = NULL;
			else if (iter2->next == iter->cur)
				iter2->next = iter->cur->next;
		}
	}

	msg = iter->cur->data;
	if (msg)
		g_object_remove_weak_pointer (G_OBJECT (msg), &iter->cur->data);

	/* If deleting the last item, fix tail */
	if (queue->tail == iter->cur)
		queue->tail = queue->tail->prev;

	/* Remove the item */
	queue->head = g_list_delete_link (queue->head, iter->cur);
	iter->cur = NULL;

	return msg;
}

void
soup_message_queue_remove_message (SoupMessageQueue *queue, SoupMessage *msg)
{
	SoupMessageQueueIter iter;
	SoupMessage *msg2;

	for (msg2 = soup_message_queue_first (queue, &iter); msg2; msg2 = soup_message_queue_next (queue, &iter)) {
		if (msg2 == msg) {
			soup_message_queue_remove (queue, &iter);
			soup_message_queue_free_iter (queue, &iter);
			return;
		}
	}
}


/**
 * soup_message_queue_free_iter:
 * @queue: a queue
 * @iter: pointer to an initialized #SoupMessageQueueIter
 *
 * Removes @iter from the list of active iterators in @queue.
 **/
void
soup_message_queue_free_iter (SoupMessageQueue *queue,
			      SoupMessageQueueIter *iter)
{
	queue->iters = g_list_remove (queue->iters, iter);
}
