/*
 *  Connline library
 *
 *  Copyright (C) 2011-2012  Intel Corporation. All rights reserved.
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

#include <stdlib.h>
#include <time.h>

static struct connline_backend_methods *connection_backend = NULL;
static DBusConnection *dbus_cnx = NULL;
static dlist *contexts_list = NULL;

static inline bool is_connline_initialized(void)
{
	if (connection_backend == NULL)
		return false;

	return true;
}

int connline_init(enum connline_event_loop event_loop_type, void *data)
{
	if (__connline_setup_event_loop(event_loop_type) < 0)
		return -EINVAL;

	dbus_cnx = __connline_setup_dbus_event_loop(data);
	if (dbus_cnx == NULL)
		return -EINVAL;

	if (connection_backend == NULL) {
		connection_backend = __connline_setup_backend(dbus_cnx);
		if (connection_backend == NULL) {
			dbus_connection_unref(dbus_cnx);
			dbus_cnx = NULL;

			return -EINVAL;
		}
	}

	srand(time(NULL));

	return 0;
}

struct connline_context *connline_new(unsigned int bearer_type)
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

	context->dbus_cnx = dbus_connection_ref(dbus_cnx);
	context->bearer_type = bearer_type;

	return context;
}

int connline_set_error_callback(struct connline_context *context,
					connline_callback_f callback)
{
	if (context == NULL)
		return -EINVAL;

	context->error_callback = callback;

	return 0;
}

int connline_set_disconnected_callback(struct connline_context *context,
						connline_callback_f callback)
{
	if (context == NULL)
		return -EINVAL;

	context->disconnected_callback = callback;

	return 0;
}

int connline_set_connected_callback(struct connline_context *context,
					connline_callback_f callback)
{
	if (context == NULL)
		return -EINVAL;

	context->connected_callback = callback;

	return 0;
}

int connline_set_property_callback(struct connline_context *context,
					connline_callback_f callback)
{
	if (context == NULL)
		return -EINVAL;

	context->property_callback = callback;

	return 0;
}

int connline_set_user_data(struct connline_context *context, void *user_data)
{
	if (context == NULL)
		return -EINVAL;

	context->user_data = user_data;

	return 0;
}

int connline_open(struct connline_context *context,
					bool background_connection)
{
	__connline_open_f __connline_open;

	if (context == NULL || is_connline_initialized() == false)
		return -EINVAL;

	context->background_connection = background_connection;

	__connline_open = connection_backend->__connline_open;
	if (__connline_open == NULL)
		return -EINVAL;

	return __connline_open(context);
}

bool connline_is_online(struct connline_context *context)
{
	if (context == NULL)
		return false;

	return context->is_online;
}

static inline void __backend_close(struct connline_context *context)
{
	__connline_close_f __connline_close;

	if (context == NULL || is_connline_initialized() == false)
		return;

	__connline_close = connection_backend->__connline_close;
	if (__connline_close != NULL)
		__connline_close(context);

	dbus_connection_unref(context->dbus_cnx);
}

void connline_close(struct connline_context *context)
{
	if (context == NULL || is_connline_initialized() == FALSE)
		return;

	__backend_close(context);

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

static void __cleanup_context(void *data)
{
	struct connline_context *context = data;

	__backend_close(context);

	free(context);
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

