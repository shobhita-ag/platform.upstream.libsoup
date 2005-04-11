/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * soup-session-sync.c
 *
 * Copyright (C) 2000-2003, Ximian, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "soup-session-sync.h"
#include "soup-connection.h"

typedef struct {
	GMutex *lock;
	GCond *cond;
} SoupSessionSyncPrivate;
#define SOUP_SESSION_SYNC_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), SOUP_TYPE_SESSION_SYNC, SoupSessionSyncPrivate))

void         queue_message  (SoupSession *session, SoupMessage *msg,
			     SoupMessageCallbackFn callback,
			     gpointer user_data);
static guint send_message   (SoupSession *session, SoupMessage *msg);
static void  cancel_message (SoupSession *session, SoupMessage *msg);

G_DEFINE_TYPE (SoupSessionSync, soup_session_sync, SOUP_TYPE_SESSION)

static void
soup_session_sync_init (SoupSessionSync *ss)
{
	SoupSessionSyncPrivate *priv = SOUP_SESSION_SYNC_GET_PRIVATE (ss);

	priv->lock = g_mutex_new ();
	priv->cond = g_cond_new ();
}

static void
finalize (GObject *object)
{
	SoupSessionSyncPrivate *priv = SOUP_SESSION_SYNC_GET_PRIVATE (object);

	g_mutex_free (priv->lock);
	g_cond_free (priv->cond);

	G_OBJECT_CLASS (soup_session_sync_parent_class)->finalize (object);
}

static void
soup_session_sync_class_init (SoupSessionSyncClass *session_sync_class)
{
	GObjectClass *object_class = G_OBJECT_CLASS (session_sync_class);
	SoupSessionClass *session_class = SOUP_SESSION_CLASS (session_sync_class);

	g_type_class_add_private (session_sync_class, sizeof (SoupSessionSyncPrivate));

	/* virtual method override */
	session_class->queue_message = queue_message;
	session_class->send_message = send_message;
	session_class->cancel_message = cancel_message;
	object_class->finalize = finalize;
}


SoupSession *
soup_session_sync_new (void)
{
	return g_object_new (SOUP_TYPE_SESSION_SYNC, NULL);
}

SoupSession *
soup_session_sync_new_with_options (const char *optname1, ...)
{
	SoupSession *session;
	va_list ap;

	va_start (ap, optname1);
	session = (SoupSession *)g_object_new_valist (SOUP_TYPE_SESSION_SYNC,
						      optname1, ap);
	va_end (ap);

	return session;
}


void
queue_message (SoupSession *session, SoupMessage *msg,
	       SoupMessageCallbackFn callback, gpointer user_data)
{
	/* FIXME */
	g_warning ("soup_session_queue_message called on synchronous session");
}

static SoupConnection *
wait_for_connection (SoupSession *session, SoupMessage *msg)
{
	SoupSessionSyncPrivate *priv = SOUP_SESSION_SYNC_GET_PRIVATE (session);
	SoupConnection *conn;
	gboolean try_pruning = FALSE, is_new = FALSE;
	guint status;

	g_mutex_lock (priv->lock);

 try_again:
	conn = soup_session_get_connection (session, msg,
					    &try_pruning, &is_new);
	if (conn) {
		if (is_new) {
			status = soup_connection_connect_sync (conn);

			/* If the connection attempt fails, SoupSession
			 * will notice, unref conn, and set an error
			 * status on msg. So all we need to do is just
			 * not return the no-longer-valid connection.
			 */

			if (!SOUP_STATUS_IS_SUCCESSFUL (status))
				conn = NULL;
			else if (msg->status == SOUP_MESSAGE_STATUS_FINISHED) {
				/* Message was cancelled while we were
				 * connecting.
				 */
				soup_connection_disconnect (conn);
				conn = NULL;
			}
		}

		g_mutex_unlock (priv->lock);
		return conn;
	}

	if (try_pruning && soup_session_try_prune_connection (session))
		goto try_again;

	/* Wait... */
	g_cond_wait (priv->cond, priv->lock);

	/* See if something bad happened */
	if (msg->status == SOUP_MESSAGE_STATUS_FINISHED) {
		g_mutex_unlock (priv->lock);
		return NULL;
	}

	goto try_again;
}

static guint
send_message (SoupSession *session, SoupMessage *msg)
{
	SoupSessionSyncPrivate *priv = SOUP_SESSION_SYNC_GET_PRIVATE (session);
	SoupConnection *conn;

	SOUP_SESSION_CLASS (soup_session_sync_parent_class)->queue_message (session, msg, NULL, NULL);

	do {
		/* Get a connection */
		conn = wait_for_connection (session, msg);
		if (!conn)
			return msg->status_code;

		soup_connection_send_request (conn, msg);
		g_cond_broadcast (priv->cond);
	} while (msg->status != SOUP_MESSAGE_STATUS_FINISHED);

	return msg->status_code;
}

static void
cancel_message (SoupSession *session, SoupMessage *msg)
{
	SoupSessionSyncPrivate *priv = SOUP_SESSION_SYNC_GET_PRIVATE (session);

	SOUP_SESSION_CLASS (soup_session_sync_parent_class)->cancel_message (session, msg);
	g_cond_broadcast (priv->cond);
}

