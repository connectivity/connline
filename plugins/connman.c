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
#include <connline/event.h>
#include <connline/utils.h>
#include <connline/dbus.h>
#include <connline/backend.h>

#include <dbus/dbus.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define CONNMAN_DBUS_NAME "net.connman"

#define CONNMAN_MANAGER_PATH "/"
#define CONNMAN_MANAGER_INTERFACE CONNMAN_DBUS_NAME ".Manager"
#define CONNMAN_NOTIFICATION_INTERFACE CONNMAN_DBUS_NAME ".Notification"
#define CONNMAN_SESSION_INTERFACE CONNMAN_DBUS_NAME ".Session"

#define CONNMAN_SERVICE_MATCH_RULE "type='signal'" \
				",sender='" DBUS_INTERFACE_DBUS "'" \
				",interface='" DBUS_INTERFACE_DBUS "'" \
				",member='" DBUS_SERVICE_OWNER_CHANGED "'" \
				",arg0='" CONNMAN_DBUS_NAME "'"

struct connman_dbus {
	char *session_name;
	char *session_path;
	enum connline_bearer bearer;

	char *notifier_path;
	struct DBusObjectPathVTable notification;

	DBusPendingCall *call;
};

struct connman_dbus_method {
	const char *method;
	const char *signature;

	DBusObjectPathMessageFunction function;
};

const char *connline_backend_watch_rule = CONNMAN_SERVICE_MATCH_RULE;
const char *connline_backend_service_name = CONNMAN_DBUS_NAME;

static void free_connman_dbus(struct connman_dbus *connman)
{
	if (connman == NULL)
		return;

	free(connman->session_name);
	free(connman->session_path);
	free(connman->notifier_path);

	free(connman);
}

static int connman_connect(struct connline_context *context)
{
	int ret = -EINVAL;
	struct connman_dbus *connman;
	DBusMessage *message;

	if (context->background_connection == TRUE)
		return 0;

	connman = context->backend_data;

	message = dbus_message_new_method_call(CONNMAN_DBUS_NAME,
						connman->session_path,
						CONNMAN_SESSION_INTERFACE,
						"Connect");
	if (message == NULL) {
		ret = -ENOMEM;
		goto error;
	}

	if (dbus_connection_send(context->dbus_cnx, message, NULL) == FALSE)
		goto error;

	dbus_message_unref(message);

	return 0;

error:
	if (message != NULL)
		dbus_message_unref(message);

	return ret;
}

static void close_notification(struct connline_context *context)
{
	struct connman_dbus *connman = context->backend_data;
	char *rule;
	int length;

	if (connman == NULL || connman->notifier_path == NULL)
		return;

	dbus_connection_unregister_object_path(context->dbus_cnx,
						connman->notifier_path);

	length = strlen(connman->notifier_path) + 60;

	rule = calloc(length, sizeof(char));
	if (rule == NULL)
		return;

	if (snprintf(rule, length, "type='signal',interface='%s',path='%s'",
					CONNMAN_NOTIFICATION_INTERFACE,
					connman->notifier_path) > 0)
		dbus_bus_remove_match(context->dbus_cnx, rule, NULL);

	free(rule);
}

static void connman_backend_data_cleanup(struct connline_context *context)
{
	struct connman_dbus *connman = context->backend_data;

	__connline_trigger_cleanup(context);

	close_notification(context);

	if (connman != NULL) {
		if (connman->call != NULL) {
			dbus_pending_call_cancel(connman->call);
			dbus_pending_call_unref(connman->call);
		}

		free_connman_dbus(connman);
		context->backend_data = NULL;
	}
}

static DBusHandlerResult notifier_release_method(DBusConnection *dbus_cnx,
						DBusMessage *message,
						void *user_data)
{
	struct connline_context *context = user_data;

	connman_backend_data_cleanup(context);
	dbus_message_unref(message);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static enum connline_bearer connman_to_connline_bearer(const char *bearer)
{
	int len;

	len = strlen(bearer);

	if (strncmp(bearer, "ethernet", len) == 0)
		return CONNLINE_BEARER_ETHERNET;
	if (strncmp(bearer, "wifi", len) == 0)
		return CONNLINE_BEARER_WIFI;
	if (strncmp(bearer, "cellular", len) == 0)
		return CONNLINE_BEARER_CELLULAR;
	if (strncmp(bearer, "wimax", len) == 0)
		return CONNLINE_BEARER_WIMAX;
	if (strncmp(bearer, "bluetooth", len) == 0)
		return CONNLINE_BEARER_BLUETOOTH;
	if (strncmp(bearer, "usb", len) == 0)
		return CONNLINE_BEARER_USB;

	return CONNLINE_BEARER_UNKNOWN;
}

static dbus_bool_t is_connected(const char *state)
{
	if (strncmp(state, "connected", 9) == 0)
		return TRUE;
	if (strncmp(state, "online", 6) == 0)
		return TRUE;

	return FALSE;
}

static dbus_bool_t is_online(const char *state)
{
	if (strncmp(state, "online", 6) == 0)
		return TRUE;

	return FALSE;
}

static DBusHandlerResult notifier_update_method(DBusConnection *dbus_cnx,
						DBusMessage *message,
						void *user_data)
{
	struct connline_context *context = user_data;
	struct connman_dbus *connman;
	char **properties = NULL;
	DBusMessageIter arg, dict;
	const char *value;

	connman = context->backend_data;

	dbus_message_iter_init(message, &arg);

	if (connline_dbus_get_dict_entry_basic(&arg, "Bearer",
					DBUS_TYPE_STRING, &value) == 0)
		connman->bearer = connman_to_connline_bearer(value);

	if (connline_dbus_get_dict_entry_basic(&arg, "State",
					DBUS_TYPE_STRING, &value) == 0) {
		if (is_connected(value) == TRUE) {
			context->is_online = is_online(value);
			__connline_call_connected_callback(context);
		} else {
			context->is_online = FALSE;
			connman->bearer = CONNLINE_BEARER_UNKNOWN;

			__connline_call_disconnected_callback(context);
		}
	}

	properties = insert_into_property_list(properties, "bearer",
				connline_bearer_to_string(connman->bearer));

	if (connline_dbus_get_dict_entry_basic(&arg, "Interface",
					DBUS_TYPE_STRING, &value) == 0) {
		properties = insert_into_property_list(properties,
							"interface", value);
	}

	if (connline_dbus_get_dict_entry_dict(&arg, "IPv4", &dict) == 0) {
		if (connline_dbus_get_dict_entry_basic(&dict, "Address",
					DBUS_TYPE_STRING, &value) == 0) {
			properties = insert_into_property_list(properties,
							"address", value);
		}
	}

	if (connline_dbus_get_dict_entry_dict(&arg, "IPv6", &dict) == 0) {
		if (connline_dbus_get_dict_entry_basic(&dict, "Address",
					DBUS_TYPE_STRING, &value) == 0) {
			properties = insert_into_property_list(properties,
							"address", value);
		}
	}

	if (properties != NULL)
		__connline_call_property_callback(context, properties);

	dbus_message_unref(message);

	return DBUS_HANDLER_RESULT_HANDLED;
}

static const struct connman_dbus_method notifier_object_methods[] = {
	{ "Release", "", notifier_release_method },
	{ "Update", "a{sv}", notifier_update_method },
	{ NULL }
};

static DBusHandlerResult notification_callback(DBusConnection *dbus_cnx,
						DBusMessage *message,
						void *user_data)
{
	const struct connman_dbus_method *handler;
	const char *interface;

	interface = dbus_message_get_interface(message);

	for (handler = notifier_object_methods; handler != NULL; handler++) {
		if (dbus_message_is_method_call(message,
					interface, handler->method) == FALSE)
			continue;

		if (dbus_message_has_signature(message,
					handler->signature) == FALSE)
			continue;

		return handler->function(dbus_cnx, message, user_data);
	}

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int setup_notification(struct connline_context *context)
{
	struct connman_dbus *connman = context->backend_data;
	DBusError error;
	int ret = 0;
	int length;
	char *rule;

	connman->notification.message_function = &notification_callback;

	length = strlen(connman->notifier_path) + 60;

	rule = calloc(length, sizeof(char));
	if (rule == NULL) {
		ret = -ENOMEM;
		goto error;
	}

	if (snprintf(rule, length, "type='signal',interface='%s',path='%s'",
					CONNMAN_NOTIFICATION_INTERFACE,
					connman->notifier_path) < 0) {
		ret = -ENOMEM;
		goto error;
	}

	dbus_error_init(&error);

	dbus_bus_add_match(context->dbus_cnx, rule, &error);

	if (dbus_error_is_set(&error)) {
		DBG("cannot add match - %s - rule: %s",
					error.message, rule);

		dbus_error_free(&error);
		ret = -EINVAL;

		goto error;
	}

	if (dbus_connection_register_object_path(context->dbus_cnx,
						connman->notifier_path,
						&connman->notification,
						(void*) context) == FALSE)
		ret = -EINVAL;

error:
	if (ret < 0) {
		free(connman->notifier_path);
		connman->notifier_path = NULL;
	}

	free(rule);

	return ret;
}

static void append_allowed_bearers(DBusMessageIter *iter, void *user_data)
{
	struct connline_context *context = user_data;
	const char *bearer;
	unsigned int value;
	int steps = 0;

	for (steps = 0; steps < 7; steps++) {
		value = 1 << steps;

		if (context->bearer_type & value) {
			bearer = connline_bearer_to_string(value);

			dbus_message_iter_append_basic(iter,
						DBUS_TYPE_STRING, &bearer);

			/* Unkown means any bearer, so we are done */
			if (steps == 0)
				return;
		}
	}
}

static void append_session_settings(DBusMessageIter *iter, void *user_data)
{
	connline_dbus_append_dict_entry_array(iter, "AllowedBearers",
			DBUS_TYPE_STRING, append_allowed_bearers, user_data);
}

static void create_session_callback(DBusPendingCall *pending, void *user_data)
{
	struct connline_context *context = user_data;
	const char *session_path = NULL;
	struct connman_dbus *connman;
	DBusMessageIter arg;
	DBusMessage *reply;
	int length;

	if (dbus_pending_call_get_completed(pending) == FALSE)
		return;

	connman = context->backend_data;
	connman->call = NULL;

	reply = dbus_pending_call_steal_reply(pending);
	if (reply == NULL)
		goto error;

	if (dbus_message_iter_init(reply, &arg) == FALSE)
		goto error;

	if (connline_dbus_get_basic(&arg, DBUS_TYPE_OBJECT_PATH,
						&session_path) != 0)
		goto error;

	length = strlen(session_path) + 1;

	if (connman->session_path != NULL)
		free(connman->session_path);

	connman->session_path = calloc(length, sizeof(char));
	if (connman->session_path == NULL)
		goto error;

	strncpy(connman->session_path, session_path, length);

	DBG("%p - %s - %s", context, connman->session_path,
						connman->notifier_path);

	dbus_message_unref(reply);
	dbus_pending_call_unref(pending);

	if (connman_connect(context) == 0)
		return;

error:
	if (reply != NULL)
		dbus_message_unref(reply);

	dbus_pending_call_unref(pending);

	connman_backend_data_cleanup(context);
	__connline_call_error_callback(context, false);
}

static int connman_create_session(struct connline_context *context)
{
	struct connman_dbus *connman = context->backend_data;
	DBusMessage *message = NULL;
	int ret = -EINVAL;
	DBusMessageIter arg;
	size_t length;

	connman->session_name = get_new_unique_name();
	if (connman->session_name == NULL)
		goto error;

	length = strlen(connman->session_name) + 2;

	/* Notifier path = /<session_name> */
	connman->notifier_path = calloc(length, sizeof(char));
	if (connman->notifier_path == NULL) {
		ret = -ENOMEM;
		goto error;
	}

	if (snprintf(connman->notifier_path, length, "/%s",
						connman->session_name) < 0)
		goto error;

	ret = setup_notification(context);
	if (ret < 0)
		goto error;

	message = dbus_message_new_method_call(CONNMAN_DBUS_NAME,
						CONNMAN_MANAGER_PATH,
						CONNMAN_MANAGER_INTERFACE,
						"CreateSession");
	if (message == NULL) {
		ret = -ENOMEM;
		goto error;
	}

	dbus_message_iter_init_append(message, &arg);

	connline_dbus_append_dict(&arg, NULL,
				append_session_settings, context);

	connline_dbus_append_basic(&arg, NULL,
			DBUS_TYPE_OBJECT_PATH, &connman->notifier_path);

	if (dbus_connection_send_with_reply(context->dbus_cnx, message,
			&connman->call, DBUS_TIMEOUT_USE_DEFAULT) == FALSE)
		goto error;

	if (dbus_pending_call_set_notify(connman->call,
			create_session_callback, context, NULL) == FALSE)
		goto error;

	dbus_message_unref(message);

	return 0;

error:
	if (message != NULL)
		dbus_message_unref(message);

	connman_backend_data_cleanup(context);

	return ret;
}

static int connman_open(struct connline_context *context)
{
	struct connman_dbus *connman;

	DBG("context %p", context);

	if (context == NULL || context->dbus_cnx == NULL)
		return -EINVAL;

	if (connline_dbus_is_service_running(context->dbus_cnx,
						CONNMAN_DBUS_NAME) == FALSE)
		return -EINVAL;

	connman = context->backend_data;

	if (connman == NULL) {
		connman = calloc(1, sizeof(struct connman_dbus));
		if (connman == NULL)
			return -ENOMEM;

		connman->bearer = CONNLINE_BEARER_UNKNOWN;
		context->backend_data = connman;
	}

	if (connman->session_name != NULL)
		return connman_connect(context);

	return connman_create_session(context);
}

static int connman_close(struct connline_context *context)
{
	struct connman_dbus *connman;
	DBusMessage *message;

	DBG("");

	if (context == NULL || context->dbus_cnx == NULL)
		return -EINVAL;

	connman = context->backend_data;

	if (connman != NULL) {
		if (connman->session_path != NULL) {
			message = dbus_message_new_method_call(
						CONNMAN_DBUS_NAME,
						connman->session_path,
						CONNMAN_SESSION_INTERFACE,
						"Destroy");
			if (message != NULL) {
				dbus_connection_send(context->dbus_cnx,
							message, NULL);
				dbus_message_unref(message);
			}
		}

		connman_backend_data_cleanup(context);
	}

	return 0;
}

static enum connline_bearer connman_get_bearer(struct connline_context *context)
{
	struct connman_dbus *connman;

	if (context == NULL || context->dbus_cnx == NULL)
		return CONNLINE_BEARER_UNKNOWN;

	connman = context->backend_data;

	return connman->bearer;
}

static struct connline_backend_methods connman = {
	connman_open,
	connman_close,
	connman_get_bearer
};

struct connline_backend_methods *connline_plugin_setup_backend(void)
{
	return &connman;
}

