/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2000-2003, Ximian, Inc.
 */

#ifndef SOUP_SESSION_H
#define SOUP_SESSION_H 1

#include <libsoup/soup-message.h>

#define SOUP_TYPE_SESSION            (soup_session_get_type ())
#define SOUP_SESSION(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), SOUP_TYPE_SESSION, SoupSession))
#define SOUP_SESSION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), SOUP_TYPE_SESSION, SoupSessionClass))
#define SOUP_IS_SESSION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), SOUP_TYPE_SESSION))
#define SOUP_IS_SESSION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), SOUP_TYPE_SESSION))
#define SOUP_SESSION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), SOUP_TYPE_SESSION, SoupSessionClass))

typedef struct SoupSessionPrivate SoupSessionPrivate;

typedef struct {
	GObject parent;

	SoupSessionPrivate *priv;
} SoupSession;

typedef struct {
	GObjectClass parent_class;

} SoupSessionClass;

GType soup_session_get_type (void);

SoupSession    *soup_session_new             (void);

void            soup_session_queue_message   (SoupSession           *session,
					      SoupMessage           *req,
					      SoupMessageCallbackFn  callback,
					      gpointer               user_data);
void            soup_session_requeue_message (SoupSession           *session,
					      SoupMessage           *req);

SoupErrorClass  soup_session_send_message    (SoupSession           *session,
					      SoupMessage            *req);


#endif /* SOUP_SESSION_H */
