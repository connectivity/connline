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

#include <connline/efl.h>

#include <connline/utils.h>

#include <Ecore.h>

struct watch_handler {
	Ecore_Fd_Handler *e_handler;
	DBusConnection *dbus_cnx;
	DBusWatch *watch;
};

struct timeout_handler {
	Ecore_Timer *e_timer;
	DBusTimeout *timeout;
};

struct callback_trigger_data {
	Ecore_Timer *e_timer;
	connline_callback_f callback;
	struct connline_context *context;
	enum connline_event event;
	char **changed_property;
};

static Eina_Hash *triggers_table = NULL;

static Eina_Bool efl_dispatch_dbus(void *data)
{
	DBusConnection *dbus_cnx = data;

	dbus_connection_ref(dbus_cnx);

	while (dbus_connection_dispatch(dbus_cnx) ==
					DBUS_DISPATCH_DATA_REMAINS);

	dbus_connection_unref(dbus_cnx);

	return EINA_FALSE;
}

static Eina_Bool watch_handler_dispatch(void *data,
					Ecore_Fd_Handler *e_handler)
{
	struct watch_handler *io_handler = data;
	DBusDispatchStatus status;
	unsigned int flags = 0;

	if (ecore_main_fd_handler_active_get(e_handler, ECORE_FD_ERROR)
								== EINA_TRUE)
		flags |= DBUS_WATCH_ERROR;
	if (ecore_main_fd_handler_active_get(e_handler, ECORE_FD_READ)
								== EINA_TRUE)
		flags |= DBUS_WATCH_READABLE;
	if (ecore_main_fd_handler_active_get(e_handler, ECORE_FD_WRITE)
								== EINA_TRUE)
		flags |= DBUS_WATCH_WRITABLE;

	dbus_watch_handle(io_handler->watch, flags);

	status = dbus_connection_get_dispatch_status(io_handler->dbus_cnx);
	if (status == DBUS_DISPATCH_DATA_REMAINS)
		ecore_timer_add(0, efl_dispatch_dbus, io_handler->dbus_cnx);

	dbus_connection_unref(io_handler->dbus_cnx);

	return TRUE;
}

static void watch_handler_free(void *data)
{
	struct watch_handler *io_handler = data;

	if (io_handler == NULL)
		return;

	if (io_handler->e_handler != NULL)
		ecore_main_fd_handler_del(io_handler->e_handler);

	dbus_connection_unref(io_handler->dbus_cnx);

	free(io_handler);
}

static dbus_bool_t efl_dbus_watch_add(DBusWatch *watch, void *data)
{
	DBusConnection *dbus_cnx = data;
	Ecore_Fd_Handler_Flags io_condition;
	struct watch_handler *io_handler;
	unsigned int flags;
	int io_fd;

	if (dbus_watch_get_enabled(watch) == FALSE)
		return TRUE;

	io_handler = calloc(1, sizeof(struct watch_handler));
	if (io_handler == NULL)
		return FALSE;

	io_handler->dbus_cnx = dbus_connection_ref(dbus_cnx);
	io_handler->watch = watch;

	dbus_watch_set_data(watch, io_handler, watch_handler_free);

	io_condition = ECORE_FD_ERROR;

	flags = dbus_watch_get_flags(watch);
	if (flags & DBUS_WATCH_READABLE)
		io_condition |= ECORE_FD_READ;
	if (flags & DBUS_WATCH_WRITABLE)
		io_condition |= ECORE_FD_WRITE;

	io_fd = dbus_watch_get_unix_fd(watch);

	io_handler->e_handler = ecore_main_fd_handler_add(io_fd, io_condition,
					watch_handler_dispatch, io_handler,
					NULL, NULL);

	return TRUE;
}

static void efl_dbus_watch_remove(DBusWatch *watch, void *data)
{
	if (dbus_watch_get_enabled(watch) == TRUE)
		return;

	dbus_watch_set_data(watch, NULL, NULL);
}

static void efl_dbus_watch_toggled(DBusWatch *watch, void *data)
{
	if (dbus_watch_get_enabled(watch) == TRUE)
		efl_dbus_watch_add(watch, data);
	else
		efl_dbus_watch_remove(watch, data);
}

static void timeout_handler_free(void *data)
{
	struct timeout_handler *to_handler = data;

	if (to_handler == NULL)
		return;

	if (to_handler->e_timer != NULL)
		ecore_timer_del(to_handler->e_timer);

	free(to_handler);
}

static Eina_Bool timeout_handler_dispatch(void *data)
{
	struct timeout_handler *to_handler = data;

	dbus_timeout_handle(to_handler->timeout);

	return TRUE;
}

static dbus_bool_t efl_dbus_timeout_add(DBusTimeout *timeout, void *data)
{
	struct timeout_handler *to_handler;

	if (dbus_timeout_get_enabled(timeout) == FALSE)
		return TRUE;

	to_handler = calloc(1, sizeof(struct timeout_handler));
	if (to_handler == NULL)
		return FALSE;

	dbus_timeout_set_data(timeout, to_handler, timeout_handler_free);

	to_handler->e_timer = ecore_timer_add(
					dbus_timeout_get_interval(timeout),
					timeout_handler_dispatch, to_handler);

	return TRUE;
}

static void efl_dbus_timeout_remove(DBusTimeout *timeout, void *data)
{
	dbus_timeout_set_data(timeout, NULL, NULL);
}

static void efl_dbus_timeout_toggled(DBusTimeout *timeout, void *data)
{
	if (dbus_timeout_get_enabled(timeout) == TRUE)
		efl_dbus_timeout_add(timeout, data);
	else
		efl_dbus_timeout_remove(timeout, data);
}

static void efl_dbus_dispatch_status(DBusConnection *connection,
				DBusDispatchStatus new_status, void *data)
{
	DBusDispatchStatus status;

	if (dbus_connection_get_is_connected(connection) == FALSE)
		return;

	status = dbus_connection_get_dispatch_status(connection);
	if (status == DBUS_DISPATCH_DATA_REMAINS)
		ecore_timer_add(0, efl_dispatch_dbus, connection);
}

static dbus_bool_t setup_dbus_in_efl_mainloop(DBusConnection *dbus_cnx)
{
	DBusDispatchStatus status;

	if (dbus_connection_set_watch_functions(dbus_cnx,
			efl_dbus_watch_add, efl_dbus_watch_remove,
			efl_dbus_watch_toggled, dbus_cnx, NULL) == FALSE)
		return FALSE;

	if (dbus_connection_set_timeout_functions(dbus_cnx,
			efl_dbus_timeout_add, efl_dbus_timeout_remove,
			efl_dbus_timeout_toggled, dbus_cnx, NULL) == FALSE)
		return FALSE;

	dbus_connection_set_dispatch_status_function(dbus_cnx,
			efl_dbus_dispatch_status, dbus_cnx, NULL);

	status = dbus_connection_get_dispatch_status(dbus_cnx);
	if (status == DBUS_DISPATCH_DATA_REMAINS)
		ecore_timer_add(0, efl_dispatch_dbus, dbus_cnx);

	return TRUE;
}

static Eina_Bool trigger_run(void *data)
{
	struct callback_trigger_data *trigger = data;
	struct connline_context *context;
	connline_callback_f callback;
	enum connline_event event;
	char **changed_property;
	Ecore_Timer *e_timer;
	Eina_Hash *context_ht;

	if (trigger == NULL)
		return EINA_FALSE;

	callback = trigger->callback;
	context = trigger->context;
	event = trigger->event;

	changed_property = trigger->changed_property;
	trigger->changed_property = NULL;

	e_timer = trigger->e_timer;
	trigger->e_timer = NULL;

	callback(context, event, changed_property, context->user_data);

	context_ht = eina_hash_find(triggers_table, context);
	if (context_ht != NULL)
		eina_hash_del(context_ht, e_timer, NULL);

	return FALSE;
}


static void remove_trigger(void *data)
{
	 struct callback_trigger_data *trigger = data;

	 if (trigger->e_timer != NULL)
		 ecore_timer_del(trigger->e_timer);

	 if (trigger->changed_property != NULL)
		 property_list_free(trigger->changed_property);

	 free(trigger);
}

static void remove_context_triggers(void *data)
{
	Eina_Hash *context_ht = data;

	eina_hash_free(context_ht);
}

DBusConnection *__connline_efl_setup_dbus_event_loop(void)
{
	DBusConnection *dbus_cnx;

	dbus_cnx = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);
	if (dbus_cnx == NULL)
		return NULL;

	if (setup_dbus_in_efl_mainloop(dbus_cnx) == FALSE) {
		dbus_connection_unref(dbus_cnx);
		return NULL;
	}

	if (triggers_table == NULL)
		triggers_table = eina_hash_pointer_new(
						remove_context_triggers);

	return dbus_cnx;
}

int __connline_efl_trigger_callback(struct connline_context *context,
						connline_callback_f callback,
						enum connline_event event,
						char **changed_property)
{
	struct callback_trigger_data *trigger;
	Eina_Hash *context_ht;

	if (callback == NULL || context == NULL)
		return -EINVAL;

	trigger = calloc(1, sizeof(struct callback_trigger_data));
	if (trigger == NULL)
		return -ENOMEM;

	trigger->callback = callback;
	trigger->context = context;
	trigger->event = event;
	trigger->changed_property = changed_property;

	trigger->e_timer = ecore_timer_add(0, trigger_run, trigger);

	context_ht = eina_hash_find(triggers_table, context);
	if (context_ht == NULL) {
		context_ht = eina_hash_pointer_new(remove_trigger);

		eina_hash_add(triggers_table, context, context_ht);
	}

	eina_hash_add(context_ht, trigger->e_timer, trigger);

	return 0;
}

void __connline_efl_trigger_cleanup(struct connline_context *context)
{
	if (context == NULL)
		return;

	eina_hash_del(triggers_table, context, NULL);
}

void __connline_efl_cleanup(DBusConnection *dbus_cnx)
{
	if (triggers_table != NULL)
		eina_hash_free(triggers_table);
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

