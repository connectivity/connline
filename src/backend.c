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

#include <connline/backend.h>
#include <connline/list.h>
#include <connline/dbus.h>
#include <connline/private.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static dlist *backends_list = NULL;
static DBusConnection *dbus = NULL;
struct connline_backend_methods *connection_backend = NULL;

static DBusHandlerResult watch_service_callback(DBusConnection *dbus_cnx,
							DBusMessage *message,
							void *user_data)
{
	struct connline_backend_plugin *backend = user_data;
	const char *member, *name, *old_owner, *new_owner;

	if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	member = dbus_message_get_member(message);
	if (member == NULL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (strncmp(member, DBUS_SERVICE_OWNER_CHANGED,
				sizeof(DBUS_SERVICE_OWNER_CHANGED)) != 0)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (dbus_message_get_args(message, NULL, DBUS_TYPE_STRING,
					&name, DBUS_TYPE_STRING, &old_owner,
					DBUS_TYPE_STRING, &new_owner,
					DBUS_TYPE_INVALID) == FALSE)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (strncmp(name, backend->service_name,
					sizeof(backend->service_name)) != 0)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (connline_dbus_is_service_running(dbus,
					backend->service_name) == TRUE) {
		if (connection_backend == NULL)
			connection_backend = backend->setup();

		if (connection_backend == NULL) {
			perror("Connline fatal error: no recovery\n");
			__connline_invalidate_contexts();

			return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
		}

		__connline_reconnect_contexts();
	} else {
		__connline_disconnect_contexts();
		connection_backend = NULL;
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

int __connline_setup_backend(DBusConnection *dbus_cnx)
{
	int ret = 0;

	if (dbus_cnx == NULL)
		return -EINVAL;

	dbus = dbus_cnx;

	ret = __connline_load_backend_plugins();
	if (ret < 0) {
		__connline_cleanup_backend();
		dbus = NULL;

		return ret;
	}

	return 0;
}

int __connline_backend_add(struct connline_backend_plugin *backend_plugin)
{
	dlist *new_list;
	int ret;

	ret = connline_dbus_setup_watch(dbus, backend_plugin->watch_rule,
				watch_service_callback, backend_plugin);
	if (ret < 0)
		return ret;

	new_list = dlist_prepend(backends_list, backend_plugin);
	if (new_list == backends_list)
		return -ENOMEM;

	backends_list = new_list;

	if (connline_dbus_is_service_running(dbus,
				backend_plugin->service_name) == TRUE) {
		/* -ENOEXEC is a fatal error for connline */
		if (connection_backend != NULL)
			return -ENOEXEC;

		connection_backend = backend_plugin->setup();
		if (connection_backend == NULL)
			return -ENOEXEC;
	}

	return 0;
}

static void __cleanup_backend(void *data)
{
	struct connline_backend_plugin *backend = data;

	if (backend == NULL)
		return;

	connline_dbus_remove_watch(dbus, backend->watch_rule,
					watch_service_callback, backend);
	__connline_cleanup_backend_plugin(backend);
}

void __connline_cleanup_backend(void)
{
	dlist_foreach(backends_list, __cleanup_backend);
	backends_list = NULL;
}
