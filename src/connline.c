/*
 *  Connline library
 *
 *  Copyright (C) 2011-2013  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License version 2.1,
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <connline/data.h>
#include <connline/list.h>
#include <connline/private.h>
#include <connline/backend.h>
#include <connline/utils.h>

#include <stdlib.h>
#include <time.h>

extern struct connline_backend_methods *connection_backend;

static DBusConnection *dbus_cnx = NULL;
static dlist *contexts_list = NULL;

static inline bool is_connline_initialized(void)
{
	if (dbus_cnx == NULL)
		return false;

	return true;
}

static inline bool is_backend_up(void)
{
	if (connection_backend == NULL)
		return false;

	return true;
}

void __connline_close(struct connline_context *context)
{
	__connline_close_f _connline_close;

	if (is_backend_up() == false)
		goto out;

	_connline_close = connection_backend->__connline_close;
	_connline_close(context);

out:
	context->is_online = false;
}

static void disconnect_context(void *data)
{
	struct connline_context *context = data;

	__connline_close(context);
	__connline_call_error_callback(context, true);
}

void __connline_disconnect_contexts(void)
{
	dlist_foreach(contexts_list, disconnect_context);
}

static void reconnect_context(void *data)
{
	struct connline_context *context = data;
	__connline_open_f _connline_open;

	_connline_open = connection_backend->__connline_open;
	_connline_open(context);
}

void __connline_reconnect_contexts(void)
{
	dlist_foreach(contexts_list, reconnect_context);
}

static void invalidate_context(void *data)
{
	struct connline_context *context = data;

	__connline_call_error_callback(context, false);
}

void __connline_invalidate_contexts(void)
{
	dlist_foreach(contexts_list, invalidate_context);
}

static void __cleanup_context(void *data)
{
	struct connline_context *context = data;

	__connline_close(context);
	free(context);
}

int connline_init(enum connline_event_loop event_loop_type, void *data)
{
	int ret = 0;

	if (__connline_setup_event_loop(event_loop_type) < 0)
		return -EINVAL;

	dbus_cnx = __connline_setup_dbus_event_loop(data);
	if (dbus_cnx == NULL)
		return -EINVAL;

	ret = __connline_setup_backend(dbus_cnx);
	if (ret < 0) {
		dbus_connection_unref(dbus_cnx);
		dbus_cnx = NULL;

		return ret;
	}

	srand(time(NULL));

	return ret;
}

static struct connline_context *__connline_context_new(void)
{
	struct connline_context *context;
	dlist *new_list;

	context = calloc(1, sizeof(struct connline_context));
	if (context == NULL)
		return NULL;

	new_list = dlist_prepend(contexts_list, context);
	if (new_list == contexts_list) {
		free(context);
		return NULL;
	}

	contexts_list = new_list;

	return context;
}

struct connline_context *connline_open(enum connline_bearer bearer_type,
						bool background_connection,
						connline_callback_f callback,
						void *user_data)
{
	struct connline_context *context;
	__connline_open_f _connline_open;

	if (is_connline_initialized() == false)
		return NULL;

	context = __connline_context_new();
	if (context == NULL)
		return NULL;

	context->bearer_type = bearer_type;
	context->background_connection = background_connection;
	context->event_callback = callback;
	context->user_data = user_data;
	context->dbus_cnx = dbus_connection_ref(dbus_cnx);

	if (is_backend_up() == false)
		return context;

	_connline_open = connection_backend->__connline_open;
	_connline_open(context);

	return context;
}

bool connline_is_online(struct connline_context *context)
{
	if (context == NULL)
		return false;

	return context->is_online;
}

void connline_close(struct connline_context *context)
{
	if (context == NULL || is_connline_initialized() == false)
		return;

	__connline_close(context);
	dbus_connection_unref(context->dbus_cnx);

	contexts_list = dlist_remove(contexts_list, context);
	free(context);
}

enum connline_bearer connline_get_bearer(struct connline_context *context)
{
	__connline_get_bearer_f __connline_get_bearer;

	if (context == NULL || is_connline_initialized() == false)
		return CONNLINE_BEARER_UNKNOWN;

	__connline_get_bearer = connection_backend->__connline_get_bearer;
	if (__connline_get_bearer == NULL)
		return CONNLINE_BEARER_UNKNOWN;

	return __connline_get_bearer(context);
}

void connline_cleanup(void)
{
	dlist_foreach(contexts_list, __cleanup_context);
	dlist_free_all(contexts_list);
	contexts_list = NULL;

	__connline_cleanup_event_loop(dbus_cnx);

	if (dbus_cnx != NULL)
		dbus_connection_unref(dbus_cnx);

	dbus_cnx = NULL;
}

