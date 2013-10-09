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

#include <connline/connline.h>
#include <connline/data.h>
#include <connline/utils.h>

#include <errno.h>
#include <dbus/dbus.h>
#include <glib.h>
#include <stdlib.h>

const unsigned int connline_plugin_event_loop_type = CONNLINE_EVENT_LOOP_GLIB;

struct watch_handler {
	unsigned int id;
	DBusConnection *dbus_cnx;
	DBusWatch *watch;
};

struct timeout_handler {
	unsigned int id;
	DBusTimeout *timeout;
};

struct callback_trigger_data {
	unsigned int source_id;
	connline_callback_f callback;
	struct connline_context *context;
	enum connline_event event;
	char **changed_property;
};

static GHashTable *triggers_table = NULL;

static gboolean glib_dispatch_dbus(gpointer data)
{
	DBusConnection *dbus_cnx = data;

	dbus_connection_ref(dbus_cnx);

	while (dbus_connection_dispatch(dbus_cnx) ==
					DBUS_DISPATCH_DATA_REMAINS);

	dbus_connection_unref(dbus_cnx);

	return FALSE;
}

static gboolean watch_handler_dispatch(GIOChannel *source,
					GIOCondition condition, gpointer data)
{
	struct watch_handler *io_handler = data;
	DBusDispatchStatus status;
	unsigned int flags = 0;

	dbus_connection_ref(io_handler->dbus_cnx);

	if (condition & G_IO_ERR)
		flags |= DBUS_WATCH_ERROR;
	if (condition & G_IO_HUP)
		flags |= DBUS_WATCH_HANGUP;
	if (condition & G_IO_IN)
		flags |= DBUS_WATCH_READABLE;
	if (condition & G_IO_OUT)
		flags |= DBUS_WATCH_WRITABLE;

	dbus_watch_handle(io_handler->watch, flags);

	status = dbus_connection_get_dispatch_status(io_handler->dbus_cnx);
	if (status == DBUS_DISPATCH_DATA_REMAINS)
		g_timeout_add(0, glib_dispatch_dbus, io_handler->dbus_cnx);

	dbus_connection_unref(io_handler->dbus_cnx);

	return TRUE;
}

static void watch_handler_free(void *data)
{
	struct watch_handler *io_handler = data;

	if (io_handler == NULL)
		return;

	if (io_handler->id > 0)
		g_source_remove(io_handler->id);

	dbus_connection_unref(io_handler->dbus_cnx);

	free(io_handler);
}

static dbus_bool_t glib_dbus_watch_add(DBusWatch *watch, void *data)
{
	DBusConnection *dbus_cnx = data;
	GIOCondition io_condition;
	GIOChannel *io_channel;
	struct watch_handler *io_handler;
	unsigned int flags;

	if (dbus_watch_get_enabled(watch) == FALSE)
		return TRUE;

	io_handler = calloc(1, sizeof(struct watch_handler));
	if (io_handler == NULL)
		return FALSE;

	io_handler->dbus_cnx = dbus_connection_ref(dbus_cnx);
	io_handler->watch = watch;

	dbus_watch_set_data(watch, io_handler, watch_handler_free);

	flags = dbus_watch_get_flags(watch);

	io_condition = G_IO_ERR | G_IO_HUP;

	if (flags & DBUS_WATCH_READABLE)
		io_condition |= G_IO_IN;
	if (flags & DBUS_WATCH_WRITABLE)
		io_condition |= G_IO_OUT;

	io_channel = g_io_channel_unix_new(dbus_watch_get_unix_fd(watch));

	io_handler->id = g_io_add_watch(io_channel, io_condition,
					watch_handler_dispatch, io_handler);

	g_io_channel_unref(io_channel);

	return TRUE;
}

static void glib_dbus_watch_remove(DBusWatch *watch, void *data)
{
	if (dbus_watch_get_enabled(watch) == TRUE)
		return;

	dbus_watch_set_data(watch, NULL, NULL);
}

static void glib_dbus_watch_toggled(DBusWatch *watch, void *data)
{
	if (dbus_watch_get_enabled(watch) == TRUE)
		glib_dbus_watch_add(watch, data);
	else
		glib_dbus_watch_remove(watch, data);
}

static void timeout_handler_free(void *data)
{
	struct timeout_handler *to_handler = data;

	if (to_handler == NULL)
		return;

	if (to_handler->id > 0)
		g_source_remove(to_handler->id);

	free(to_handler);
}

static gboolean timeout_handler_dispatch(gpointer data)
{
	struct timeout_handler *to_handler = data;

	dbus_timeout_handle(to_handler->timeout);

	return FALSE;
}

static dbus_bool_t glib_dbus_timeout_add(DBusTimeout *timeout, void *data)
{
	struct timeout_handler *to_handler;

	if (dbus_timeout_get_enabled(timeout) == FALSE)
		return TRUE;

	to_handler = calloc(1, sizeof(struct timeout_handler));
	if (to_handler == NULL)
		return FALSE;

	dbus_timeout_set_data(timeout, to_handler, timeout_handler_free);

	to_handler->id = g_timeout_add(dbus_timeout_get_interval(timeout),
					timeout_handler_dispatch, to_handler);

	return TRUE;
}

static void glib_dbus_timeout_remove(DBusTimeout *timeout, void *data)
{
	dbus_timeout_set_data(timeout, NULL, NULL);
}

static void glib_dbus_timeout_toggled(DBusTimeout *timeout, void *data)
{
	if (dbus_timeout_get_enabled(timeout) == TRUE)
		glib_dbus_timeout_add(timeout, data);
	else
		glib_dbus_timeout_remove(timeout, data);
}

static void glib_dbus_dispatch_status(DBusConnection *connection,
				DBusDispatchStatus new_status, void *data)
{
	DBusDispatchStatus status;

	if (dbus_connection_get_is_connected(connection) == FALSE)
		return;

	status = dbus_connection_get_dispatch_status(connection);
	if (status == DBUS_DISPATCH_DATA_REMAINS)
		g_timeout_add(0, glib_dispatch_dbus, connection);
}

static dbus_bool_t setup_dbus_in_glib_mainloop(DBusConnection *dbus_cnx)
{
	DBusDispatchStatus status;

	if (dbus_connection_set_watch_functions(dbus_cnx,
			glib_dbus_watch_add, glib_dbus_watch_remove,
			glib_dbus_watch_toggled, dbus_cnx, NULL) == FALSE)
		return FALSE;

	if (dbus_connection_set_timeout_functions(dbus_cnx,
			glib_dbus_timeout_add, glib_dbus_timeout_remove,
			glib_dbus_timeout_toggled, dbus_cnx, NULL) == FALSE)
		return FALSE;

	dbus_connection_set_dispatch_status_function(dbus_cnx,
			glib_dbus_dispatch_status, dbus_cnx, NULL);

	status = dbus_connection_get_dispatch_status(dbus_cnx);
	if (status == DBUS_DISPATCH_DATA_REMAINS)
		g_timeout_add(0, glib_dispatch_dbus, dbus_cnx);

	return TRUE;
}

static gboolean trigger_run(gpointer data)
{
	struct callback_trigger_data *trigger = data;
	struct connline_context *context;
	connline_callback_f callback;
	enum connline_event event;
	char **changed_property;
	unsigned int source_id;
	GHashTable *context_ht;

	if (trigger == NULL)
		return FALSE;

	callback = trigger->callback;
	context = trigger->context;
	event = trigger->event;

	changed_property = trigger->changed_property;
	trigger->changed_property = NULL;

	source_id = trigger->source_id;
	trigger->source_id = 0;

	context_ht = g_hash_table_lookup(triggers_table, context);
	if (context_ht != NULL)
		g_hash_table_remove(context_ht, GINT_TO_POINTER(source_id));

	callback(context, event, changed_property, context->user_data);

	return FALSE;
}

static void remove_trigger(gpointer data)
{
	struct callback_trigger_data *trigger = data;

	if (trigger->source_id != 0)
		g_source_remove(trigger->source_id);

	if (trigger->changed_property != NULL)
		property_list_free(trigger->changed_property);

	free(trigger);
}

static void remove_context_triggers(gpointer data)
{
	GHashTable *context_ht = data;

	g_hash_table_destroy(context_ht);
}

DBusConnection *connline_plugin_setup_event_loop(void)
{
	DBusConnection *dbus_cnx;

	dbus_cnx = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (dbus_cnx == NULL)
		return NULL;

	if (setup_dbus_in_glib_mainloop(dbus_cnx) == FALSE) {
		dbus_connection_unref(dbus_cnx);
		return NULL;
	}

	if (triggers_table == NULL)
		triggers_table = g_hash_table_new_full(NULL, NULL,
					NULL, remove_context_triggers);

	return dbus_cnx;
}

int connline_plugin_trigger_callback(struct connline_context *context,
						connline_callback_f callback,
						enum connline_event event,
						char **changed_property)
{
	struct callback_trigger_data *trigger;
	GHashTable *context_ht;

	if (callback == NULL || context == NULL)
		return -EINVAL;

	trigger = calloc(1, sizeof(struct callback_trigger_data));
	if (trigger == NULL)
		return -ENOMEM;

	trigger->callback = callback;
	trigger->context = context;
	trigger->event = event;
	trigger->changed_property = changed_property;

	trigger->source_id = g_timeout_add_full(G_PRIORITY_DEFAULT, 0,
						trigger_run, trigger, NULL);

	context_ht = g_hash_table_lookup(triggers_table, context);
	if (context_ht == NULL) {
		context_ht = g_hash_table_new_full(NULL, NULL,
						NULL, remove_trigger);

		g_hash_table_insert(triggers_table, context, context_ht);
	}

	g_hash_table_replace(context_ht,
				GINT_TO_POINTER(trigger->source_id), trigger);

	return 0;
}

void connline_plugin_trigger_cleanup(struct connline_context *context)
{
	if (context == NULL)
		return;

	g_hash_table_remove(triggers_table, context);
}

void connline_plugin_cleanup_event_loop(DBusConnection *dbus_cnx)
{
	if (triggers_table != NULL)
		g_hash_table_destroy(triggers_table);
	triggers_table = NULL;

	if (dbus_cnx == NULL)
		return;

	dbus_connection_set_watch_functions(dbus_cnx,
						NULL, NULL, NULL, NULL, NULL);
	dbus_connection_set_timeout_functions(dbus_cnx,
						NULL, NULL, NULL, NULL, NULL);
	dbus_connection_set_dispatch_status_function(dbus_cnx,
							NULL, NULL, NULL);
}

