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
#include <connline/event.h>
#include <connline/dbus.h>
#include <connline/utils.h>
#include <connline/backend.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define WICD_DBUS_NAME "org.wicd.daemon"
#define WICD_MANAGER_PATH "/org/wicd/daemon"

#define WICD_STATUS_MATCH_RULE "type='signal'" \
			",interface='" WICD_DBUS_NAME "'" \
			",member='StatusChanged'"

#define DBUS_SERVICE_OWNER_CHANGED "NameOwnerChanged"

#define WICD_SERVICE_MATCH_RULE "type='signal'" \
			",sender='" DBUS_INTERFACE_DBUS "'" \
			",interface='" DBUS_INTERFACE_DBUS "'" \
			",member='" DBUS_SERVICE_OWNER_CHANGED "'" \
			",arg0='" WICD_DBUS_NAME "'"

enum wicd_state {
	WICD_NOT_CONNECTED = 0,
	WICD_CONNECTING    = 1,
	WICD_WIRELESS      = 2,
	WICD_WIRED         = 3,
	WICD_SUSPENDED     = 4,
};

struct wicd_dbus {
	enum connline_bearer bearer;
	char *ip;

	dbus_bool_t status_watched;
	DBusPendingCall *call;
};

static DBusHandlerResult watch_wicd_service(DBusConnection *dbus_cnx,
						DBusMessage *message,
						void *user_data);

static DBusHandlerResult watch_wicd_status(DBusConnection *dbus_cnx,
						DBusMessage *message,
						void *user_data);

static enum connline_bearer wicd_state_to_connline_bearer(enum wicd_state state)
{
	switch (state) {
	case WICD_NOT_CONNECTED:
	case WICD_CONNECTING:
		break;
	case WICD_WIRED:
		return CONNLINE_BEARER_ETHERNET;
	case WICD_WIRELESS:
		return CONNLINE_BEARER_WIFI;
	case WICD_SUSPENDED:
		break;
	}

	return CONNLINE_BEARER_UNKNOWN;
}

static dbus_bool_t is_connected(enum wicd_state state)
{
	if (state == WICD_WIRED || state == WICD_WIRELESS)
		return TRUE;

	return FALSE;
}

static void wicd_backend_data_cleanup(struct connline_context *context)
{
	struct wicd_dbus *wicd = context->backend_data;

	if (wicd == NULL)
		return;

	connline_dbus_remove_watch(context->dbus_cnx,
			WICD_SERVICE_MATCH_RULE, watch_wicd_service, context);

	if (wicd->status_watched == TRUE)
		connline_dbus_remove_watch(context->dbus_cnx,
			WICD_STATUS_MATCH_RULE, watch_wicd_status, context);

	if (wicd->call != NULL) {
		dbus_pending_call_cancel(wicd->call);
		dbus_pending_call_unref(wicd->call);
	}

	free(wicd->ip);
	free(wicd);

	context->backend_data = NULL;
}

static void wicd_interface_cb(DBusPendingCall *pending, void *user_data)
{
	struct connline_context *context = user_data;
	struct wicd_dbus *wicd;
	DBusMessageIter arg;
	DBusMessage *reply;
	const char *iface;
	char **properties;

	if (dbus_pending_call_get_completed(pending) == FALSE)
		return;

	wicd = context->backend_data;
	wicd->call = NULL;

	reply = dbus_pending_call_steal_reply(pending);
	if (reply == NULL)
		goto error;

	if (dbus_message_iter_init(reply, &arg) == FALSE)
		goto error;

	if (connline_dbus_get_basic(&arg, DBUS_TYPE_STRING, &iface) != 0)
		goto error;

	properties = NULL;

	properties = insert_into_property_list(properties, "bearer",
				connline_bearer_to_string(wicd->bearer));

	properties = insert_into_property_list(properties, "interface", iface);

	properties = insert_into_property_list(properties,
						"address", wicd->ip);

	if (properties != NULL)
		__connline_call_property_callback(context, properties);

	dbus_message_unref(reply);
	dbus_pending_call_unref(pending);

	return;

error:
	if (reply != NULL)
		dbus_message_unref(reply);

	dbus_pending_call_unref(pending);

	wicd_backend_data_cleanup(context);

	__connline_call_error_callback(context);
}

static int wicd_get_interface(struct connline_context *context)
{
	struct wicd_dbus *wicd = context->backend_data;
	DBusMessage *message = NULL;
	int ret = -EINVAL;

	if (wicd->bearer == CONNLINE_BEARER_ETHERNET) {
		message = dbus_message_new_method_call(WICD_DBUS_NAME,
							WICD_MANAGER_PATH,
							WICD_DBUS_NAME,
							"GetWiredInterface");
	} else if (wicd->bearer == CONNLINE_BEARER_WIFI) {
		message = dbus_message_new_method_call(WICD_DBUS_NAME,
						WICD_MANAGER_PATH,
						WICD_DBUS_NAME,
						"GetWirelessInterface");
	} else
		return -EINVAL;

	if (message == NULL)
		return -ENOMEM;

	if (dbus_connection_send_with_reply(context->dbus_cnx, message,
			&wicd->call, DBUS_TIMEOUT_USE_DEFAULT) == FALSE)
		goto out;

	if (dbus_pending_call_set_notify(wicd->call,
				wicd_interface_cb, context, NULL) == FALSE)
		goto out;

	ret = 0;

out:
	dbus_message_unref(message);

	return ret;
}

static int process_with_connection(struct connline_context *context,
							unsigned int state,
							char **ip,
							int len)
{
	struct wicd_dbus *wicd = context->backend_data;

	wicd->bearer = wicd_state_to_connline_bearer(state);

	if (context->bearer_type == CONNLINE_BEARER_UNKNOWN ||
				wicd->bearer & context->bearer_type) {
		if (len <= 0)
			return -1;

		context->is_online = TRUE;
		wicd->ip = strdup(ip[0]);

		/* If application wants to be notified about
		 * the properties, we will handle it */
		if (context->property_callback != NULL) {
			if (wicd_get_interface(context) != 0)
				return -1;
		}

		return 1;
	}

	return 0;
}

static DBusHandlerResult watch_wicd_status(DBusConnection *dbus_cnx,
						DBusMessage *message,
						void *user_data)
{
	struct connline_context *context = user_data;
	struct wicd_dbus *wicd;
	DBusMessageIter arg;
	const char *member;
	unsigned int state;
	char **ip = NULL;
	int len, ret;

	if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	member = dbus_message_get_member(message);
	if (member == NULL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (strncmp(member, "StatusChanged", sizeof("StatusChanged")) != 0)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	wicd = context->backend_data;

	if (dbus_message_iter_init(message, &arg) == FALSE)
		goto error;

	if (connline_dbus_get_basic(&arg, DBUS_TYPE_UINT32, &state) != 0)
		goto error;

	dbus_message_iter_next(&arg);

	if (connline_dbus_get_array(&arg, DBUS_TYPE_STRING, &len, &ip) != 0)
		goto error;

	if (is_connected(state) == TRUE) {
		if (context->is_online == TRUE)
			goto out;

		ret = process_with_connection(context, state, ip, len);
		if (ret < 0)
			goto error;

		if (ret > 0)
			__connline_call_connected_callback(context);
	} else {
		if (context->is_online == FALSE)
			goto out;

		context->is_online = FALSE;

		free(wicd->ip);
		wicd->ip = NULL;
		wicd->bearer = CONNLINE_BEARER_UNKNOWN;

		__connline_call_disconnected_callback(context);
	}

out:
	free(ip);

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

error:
	free(ip);

	wicd_backend_data_cleanup(context);

	__connline_call_error_callback(context);

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void wicd_autoconnect_cb(DBusPendingCall *pending, void *user_data)
{
	struct connline_context *context = user_data;
	struct wicd_dbus *wicd;

	if (dbus_pending_call_get_completed(pending) == FALSE)
		return;

	wicd = context->backend_data;
	wicd->call = NULL;

	dbus_pending_call_unref(pending);
}

static int wicd_autoconnect(struct connline_context *context)
{
	struct wicd_dbus *wicd = context->backend_data;
	DBusMessage *message = NULL;
	dbus_bool_t fresh = TRUE;
	int ret = -EINVAL;

	message = dbus_message_new_method_call(WICD_DBUS_NAME,
						WICD_MANAGER_PATH,
						WICD_DBUS_NAME,
						"AutoConnect");
	if (message == NULL)
		return -ENOMEM;

	if (dbus_message_append_args(message, DBUS_TYPE_BOOLEAN, &fresh,
						DBUS_TYPE_INVALID) == FALSE)
		goto out;

	if (dbus_connection_send_with_reply(context->dbus_cnx, message,
			&wicd->call, DBUS_TIMEOUT_USE_DEFAULT) == FALSE)
		goto out;

	if (dbus_pending_call_set_notify(wicd->call,
			wicd_autoconnect_cb, context, NULL) == FALSE)
		goto out;

	ret = 0;

out:
	dbus_message_unref(message);

	return ret;
}

static void wicd_connection_status_cb(DBusPendingCall *pending, void *user_data)
{
	struct connline_context *context = user_data;
	DBusMessage *reply = NULL;
	struct wicd_dbus *wicd;
	DBusMessageIter arg;
	unsigned int state;
	char **ip = NULL;
	int len, ret;

	if (dbus_pending_call_get_completed(pending) == FALSE)
		return;

	wicd = context->backend_data;
	wicd->call = NULL;

	/* Watch connection status signals */
	if (connline_dbus_setup_watch(context->dbus_cnx,
				WICD_STATUS_MATCH_RULE,
				watch_wicd_status, context) != 0)
		goto error;

	wicd->status_watched = TRUE;

	reply = dbus_pending_call_steal_reply(pending);
	if (reply == NULL)
		goto error;

	if (dbus_message_iter_init(reply, &arg) == FALSE)
		goto error;

	if (connline_dbus_get_struct_entry_basic(&arg, 1,
					DBUS_TYPE_UINT32, &state) != 0)
		goto error;

	if (connline_dbus_get_struct_entry_array(&arg, 2,
					DBUS_TYPE_STRING, &len, &ip) != 0)
		goto error;

	if (is_connected(state) == TRUE) {
		ret = process_with_connection(context, state, ip, len);
		if (ret < 0)
			goto error;

		if (ret > 0)
			__connline_call_connected_callback(context);
		else
			__connline_call_disconnected_callback(context);

		goto out;
	}

	if (context->background_connection == FALSE) {
		if (wicd_autoconnect(context) != 0)
			goto error;
	} else
		__connline_call_disconnected_callback(context);

out:
	free(ip);

	dbus_message_unref(reply);
	dbus_pending_call_unref(pending);

	return;

error:
	free(ip);

	if (reply != NULL)
		dbus_message_unref(reply);

	dbus_pending_call_unref(pending);

	wicd_backend_data_cleanup(context);

	__connline_call_error_callback(context);
}

static int wicd_get_connection_status(struct connline_context *context)
{
	struct wicd_dbus *wicd = context->backend_data;
	DBusMessage *message = NULL;
	int ret = -EINVAL;

	message = dbus_message_new_method_call(WICD_DBUS_NAME,
						WICD_MANAGER_PATH,
						WICD_DBUS_NAME,
						"GetConnectionStatus");
	if (message == NULL)
		return -ENOMEM;

	if (dbus_connection_send_with_reply(context->dbus_cnx, message,
			&wicd->call, DBUS_TIMEOUT_USE_DEFAULT) == FALSE)
		goto out;

	if (dbus_pending_call_set_notify(wicd->call,
			wicd_connection_status_cb, context, NULL) == FALSE)
		goto out;

	ret = 0;

out:
	dbus_message_unref(message);

	return ret;
}

static DBusHandlerResult watch_wicd_service(DBusConnection *dbus_cnx,
						DBusMessage *message,
						void *user_data)
{
	struct connline_context *context = user_data;
	const char *unused1, *unused2;
	const char *member;
	const char *name;

	if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	member = dbus_message_get_member(message);
	if (member == NULL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (strncmp(member, DBUS_SERVICE_OWNER_CHANGED,
				sizeof(DBUS_SERVICE_OWNER_CHANGED)) != 0)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (dbus_message_get_args(message, NULL, DBUS_TYPE_STRING,
					&name, DBUS_TYPE_STRING, &unused1,
					DBUS_TYPE_STRING, &unused2,
					DBUS_TYPE_INVALID) == FALSE)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (strncmp(name, WICD_DBUS_NAME, sizeof(WICD_DBUS_NAME)) != 0)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	wicd_backend_data_cleanup(context);

	__connline_call_error_callback(context);

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int wicd_open(struct connline_context *context)
{
	struct wicd_dbus *wicd;

	if (context == NULL || context->dbus_cnx == NULL)
		return -EINVAL;

	if (connline_dbus_is_service_running(context->dbus_cnx,
						WICD_DBUS_NAME) == FALSE)
		return -EINVAL;

	wicd = context->backend_data;

	if (wicd == NULL) {
		/* Watch service */
		if (connline_dbus_setup_watch(context->dbus_cnx,
					WICD_SERVICE_MATCH_RULE,
					watch_wicd_service, context) < 0)
			return -ENOMEM;

		wicd = calloc(1, sizeof(struct wicd_dbus));
		if (wicd == NULL)
			goto error;

		context->backend_data = wicd;

		if (wicd_get_connection_status(context) < 0)
			goto error;
	} else {
		if (context->background_connection == FALSE) {
			if (wicd_autoconnect(context) < 0)
				goto error;
		}
	}

	return 0;

error:
	wicd_backend_data_cleanup(context);

	return -ENOMEM;
}

static int wicd_close(struct connline_context *context)
{
	if (context == NULL || context->dbus_cnx == NULL)
		return -EINVAL;

	wicd_backend_data_cleanup(context);

	return 0;
}

static enum connline_bearer wicd_get_bearer(struct connline_context *context)
{
	struct wicd_dbus *wicd;

	if (context == NULL || context->dbus_cnx == NULL)
		return CONNLINE_BEARER_UNKNOWN;

	wicd = context->backend_data;

	return wicd->bearer;
}

static struct connline_backend_methods wicd = {
	wicd_open,
	wicd_close,
	wicd_get_bearer
};

struct connline_backend_methods *connline_plugin_setup_backend(DBusConnection *dbus_cnx)
{
	if (connline_dbus_is_service_running(dbus_cnx, WICD_DBUS_NAME) == TRUE)
		return &wicd;

	return NULL;
}
