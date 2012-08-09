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

#include <connline/connline.h>
#include <connline/data.h>
#include <connline/utils.h>

#include <event2/event.h>
#include <event2/util.h>
#include <glib.h>
#include <errno.h>
#include <dbus/dbus.h>
#include <stdlib.h>

const unsigned int connline_plugin_event_loop_type = CONNLINE_EVENT_LOOP_LIBEVENT;

struct watch_handler {
	struct event *ev;
	DBusConnection *dbus_cnx;
	DBusWatch *watch;
};

struct timeout_handler {
	struct event *ev;
	DBusConnection *dbus_cnx;
	DBusTimeout *timeout;
};

struct callback_trigger_data {
	struct event *ev;
	connline_callback_f callback;
	struct connline_context *context;
	enum connline_event event;
	char **changed_property;
};

static struct event_base *ev_base = NULL;
static GHashTable *triggers_table = NULL;

static void timeout_handler_free(void *data)
{
	struct timeout_handler *to_handler = data;

	if (to_handler == NULL)
		return;

	if (to_handler->ev != NULL) {
		event_del(to_handler->ev);
		event_free(to_handler->ev);
	}

	if (to_handler->dbus_cnx != NULL)
		dbus_connection_unref(to_handler->dbus_cnx);

	free(to_handler);
}

static void libevent_dispatch_dbus(int fd, short event, void *data)
{
	struct timeout_handler *to_handler = data;
	DBusConnection *dbus_cnx = to_handler->dbus_cnx;

	dbus_connection_ref(dbus_cnx);

	while (dbus_connection_dispatch(dbus_cnx) ==
					DBUS_DISPATCH_DATA_REMAINS);

	dbus_connection_unref(dbus_cnx);

	timeout_handler_free(to_handler);
}

static inline void throw_libevent_dispatch_dbus(DBusConnection *dbus_cnx)
{
	const struct timeval timeout = {0,0};
	struct timeout_handler *to_handler;

	to_handler = calloc(1, sizeof(struct timeout_handler));
	if (to_handler == NULL)
		return;

	to_handler->dbus_cnx = dbus_connection_ref(dbus_cnx);

	to_handler->ev = evtimer_new(ev_base,
				libevent_dispatch_dbus, to_handler);
	evtimer_add(to_handler->ev, &timeout);
}

static void watch_handler_dispatch(int fd, short event, void *data)
{
	struct watch_handler *io_handler = data;
	DBusDispatchStatus status;
	unsigned int flags = 0;

	dbus_connection_ref(io_handler->dbus_cnx);

	if (evutil_socket_geterror(fd) != 0)
		flags |= DBUS_WATCH_ERROR;

	if (event & EV_READ)
		flags |= DBUS_WATCH_READABLE;
	if (event & EV_WRITE)
		flags |= DBUS_WATCH_WRITABLE;

	dbus_watch_handle(io_handler->watch, flags);

	status = dbus_connection_get_dispatch_status(io_handler->dbus_cnx);
	if (status == DBUS_DISPATCH_DATA_REMAINS)
		throw_libevent_dispatch_dbus(io_handler->dbus_cnx);

	dbus_connection_unref(io_handler->dbus_cnx);
}

static void watch_handler_free(void *data)
{
	struct watch_handler *io_handler = data;

	if (io_handler == NULL)
		return;

	if (io_handler->ev != NULL) {
		event_del(io_handler->ev);
		event_free(io_handler->ev);
	}

	dbus_connection_unref(io_handler->dbus_cnx);

	free(io_handler);
}

static dbus_bool_t libevent_dbus_watch_add(DBusWatch *watch, void *data)
{
	DBusConnection *dbus_cnx = data;
	struct watch_handler *io_handler;
	unsigned int flags;
	short io_condition;
	int io_fd;

	if (dbus_watch_get_enabled(watch) == FALSE)
		return TRUE;

	io_handler = calloc(1, sizeof(struct watch_handler));
	if (io_handler == NULL)
		return FALSE;

	io_handler->dbus_cnx = dbus_connection_ref(dbus_cnx);
	io_handler->watch = watch;

	dbus_watch_set_data(watch, io_handler, watch_handler_free);

	flags = dbus_watch_get_flags(watch);

	io_condition = EV_PERSIST;

	if (flags & DBUS_WATCH_READABLE)
		io_condition |= EV_READ;
	if (flags & DBUS_WATCH_WRITABLE)
		io_condition |= EV_WRITE;

	io_fd = dbus_watch_get_unix_fd(watch);

	io_handler->ev = event_new(ev_base, io_fd, io_condition,
					watch_handler_dispatch, io_handler);

	event_add(io_handler->ev, NULL);

	return TRUE;
}

static void libevent_dbus_watch_remove(DBusWatch *watch, void *data)
{
	if (dbus_watch_get_enabled(watch) == TRUE)
		return;

	dbus_watch_set_data(watch, NULL, NULL);
}

static void libevent_dbus_watch_toggled(DBusWatch *watch, void *data)
{
	if (dbus_watch_get_enabled(watch) == TRUE)
		libevent_dbus_watch_add(watch, data);
	else
		libevent_dbus_watch_remove(watch, data);
}

static void timeout_handler_dispatch(int fd, short event, void *data)
{
	struct timeout_handler *to_handler = data;

	dbus_timeout_handle(to_handler->timeout);
}

static inline void _set_timer(struct timeval *timer, long int milliseconds)
{
	timer->tv_sec = milliseconds / 1000;
	timer->tv_usec = (milliseconds % 1000) * 1000;
}

static dbus_bool_t libevent_dbus_timeout_add(DBusTimeout *timeout, void *data)
{
	struct timeout_handler *to_handler;
	struct timeval timer;

	if (dbus_timeout_get_enabled(timeout) == FALSE)
		return TRUE;

	to_handler = calloc(1, sizeof(struct timeout_handler));
	if (to_handler == NULL)
		return FALSE;

	dbus_timeout_set_data(timeout, to_handler, timeout_handler_free);

	_set_timer(&timer, dbus_timeout_get_interval(timeout));

	to_handler->ev = evtimer_new(ev_base,
				timeout_handler_dispatch, to_handler);
	evtimer_add(to_handler->ev, (const struct timeval *) &timer);

	return TRUE;
}

static void libevent_dbus_timeout_remove(DBusTimeout *timeout, void *data)
{
	dbus_timeout_set_data(timeout, NULL, NULL);
}

static void libevent_dbus_timeout_toggled(DBusTimeout *timeout, void *data)
{
	if (dbus_timeout_get_enabled(timeout) == TRUE)
		libevent_dbus_timeout_add(timeout, data);
	else
		libevent_dbus_timeout_remove(timeout, data);
}

static void libevent_dbus_dispatch_status(DBusConnection *dbus_cnx,
				DBusDispatchStatus new_status, void *data)
{
	DBusDispatchStatus status;

	if (dbus_connection_get_is_connected(dbus_cnx) == FALSE)
		return;

	status = dbus_connection_get_dispatch_status(dbus_cnx);
	if (status == DBUS_DISPATCH_DATA_REMAINS)
		throw_libevent_dispatch_dbus(dbus_cnx);
}

static dbus_bool_t setup_dbus_in_libevent_mainloop(DBusConnection *dbus_cnx)
{
	DBusDispatchStatus status;

	if (dbus_connection_set_watch_functions(dbus_cnx,
			libevent_dbus_watch_add, libevent_dbus_watch_remove,
			libevent_dbus_watch_toggled, dbus_cnx, NULL) == FALSE)
		return FALSE;

	if (dbus_connection_set_timeout_functions(dbus_cnx,
			libevent_dbus_timeout_add, libevent_dbus_timeout_remove,
			libevent_dbus_timeout_toggled, dbus_cnx, NULL) == FALSE)
		return FALSE;

	dbus_connection_set_dispatch_status_function(dbus_cnx,
			libevent_dbus_dispatch_status, dbus_cnx, NULL);

	status = dbus_connection_get_dispatch_status(dbus_cnx);
	if (status == DBUS_DISPATCH_DATA_REMAINS)
		throw_libevent_dispatch_dbus(dbus_cnx);

	return TRUE;
}

static void trigger_run(int fd, short event, void *data)
{
	struct callback_trigger_data *trigger = data;
	struct connline_context *context;
	connline_callback_f callback;
	enum connline_event c_event;
	char **changed_property;
	struct event *ev;
	GHashTable *context_ht;

	if (trigger == NULL)
		return;

	callback = trigger->callback;
	context = trigger->context;
	c_event = trigger->event;

	changed_property = trigger->changed_property;
	trigger->changed_property = NULL;

	ev = trigger->ev;
	trigger->ev = NULL;

	context_ht = g_hash_table_lookup(triggers_table, context);
	if (context_ht != NULL)
		g_hash_table_remove(context_ht, ev);

	callback(context, c_event, changed_property, context->user_data);
}

static void remove_trigger(gpointer data)
{
	struct callback_trigger_data *trigger = data;

	if (trigger->ev != NULL) {
		event_del(trigger->ev);
		event_free(trigger->ev);
	}

	if (trigger->changed_property != NULL)
		property_list_free(trigger->changed_property);

	free(trigger);
}

static void remove_context_triggers(gpointer data)
{
	GHashTable *context_ht = data;

	g_hash_table_destroy(context_ht);
}

DBusConnection *connline_plugin_setup_event_loop(void *data)
{
	DBusConnection *dbus_cnx;

	ev_base = (struct event_base *) data;
	if (ev_base == NULL)
		return NULL;

	dbus_cnx = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (dbus_cnx == NULL)
		return NULL;

	if (setup_dbus_in_libevent_mainloop(dbus_cnx) == FALSE) {
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
	const struct timeval timeout = {0,0};
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

	trigger->ev = evtimer_new(ev_base, trigger_run, trigger);
	evtimer_add(trigger->ev, &timeout);

	context_ht = g_hash_table_lookup(triggers_table, context);
	if (context_ht == NULL) {
		context_ht = g_hash_table_new_full(NULL, NULL,
						NULL, remove_trigger);

		g_hash_table_insert(triggers_table, context, context_ht);
	}

	g_hash_table_replace(context_ht, trigger->ev, trigger);

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


