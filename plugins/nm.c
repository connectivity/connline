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

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define NM_DBUS_NAME "org.freedesktop.NetworkManager"
#define NM_MANAGER_PATH "/org/freedesktop/NetworkManager"

#define DBUS_SERVICE_OWNER_CHANGED "NameOwnerChanged"

#define DBUS_FREEDESKTOP_PROPERTIES DBUS_INTERFACE_DBUS ".Properties"

#define NM_SERVICE_MATCH_RULE "type='signal'" \
			",sender='" DBUS_INTERFACE_DBUS "'" \
			",interface='" DBUS_INTERFACE_DBUS "'" \
			",member='" DBUS_SERVICE_OWNER_CHANGED "'" \
			",arg0='" NM_DBUS_NAME "'"

#define NM_STATE_SIGNAL_MATCH_RULE "type='signal'" \
			",interface='" NM_DBUS_NAME "'" \
			",member='StateChanged'"

enum nm_state {
	NM_STATE_UNKNOWN          =  0,
	NM_STATE_ASLEEP           = 10,
	NM_STATE_DISCONNECTED     = 20,
	NM_STATE_DISCONNECTING    = 30,
	NM_STATE_CONNECTING       = 40,
	NM_STATE_CONNECTED_LOCAL  = 50,
	NM_STATE_CONNECTED_SITE   = 60,
	NM_STATE_CONNECTED_GLOBAL = 70,
};

enum nm_device_type {
	NM_DEVICE_TYPE_UNKNOWN  = 0,
	NM_DEVICE_TYPE_ETHERNET = 1,
	NM_DEVICE_TYPE_WIFI     = 2,
	NM_DEVICE_TYPE_BT       = 5,
	NM_DEVICE_TYPE_WIMAX    = 7,
	NM_DEVICE_TYPE_MODEM    = 8,
};

#define NM_DEVICE_STATE_ACTIVATED 100

struct nm_dbus {
	enum nm_state state;
	enum connline_bearer bearer;

	char **devices;
	int nb_devices;
	int current_device;

	dbus_bool_t state_watched;
	DBusPendingCall *call;
};

static int nm_device_get_all(struct connline_context *context);

static DBusHandlerResult watch_nm_service(DBusConnection *dbus_cnx,
						DBusMessage *message,
						void *user_data);

static DBusHandlerResult watch_nm_state(DBusConnection *dbus_cnx,
						DBusMessage *message,
						void *user_data);

static inline void free_devices(struct nm_dbus *nm)
{
	int i;

	if (nm->devices != NULL) {
		for (i = 0; i < nm->nb_devices; free(nm->devices[i]), i++);

		free(nm->devices);
	}

	nm->devices = NULL;
	nm->nb_devices = 0;
	nm->current_device = 0;
}

static void nm_backend_data_cleanup(struct connline_context *context)
{
	struct nm_dbus *nm = context->backend_data;

	if (nm == NULL)
		return;

	connline_dbus_remove_watch(context->dbus_cnx,
			NM_SERVICE_MATCH_RULE, watch_nm_service, context);

	if (nm->state_watched == TRUE)
		connline_dbus_remove_watch(context->dbus_cnx,
			NM_STATE_SIGNAL_MATCH_RULE, watch_nm_state, context);

	if (nm->call != NULL) {
		dbus_pending_call_cancel(nm->call);
		dbus_pending_call_unref(nm->call);
	}

	free_devices(nm);

	free(nm);

	context->backend_data = NULL;
}

static enum connline_bearer nm_device_type_to_bearer(enum nm_device_type type)
{
	switch (type) {
	case NM_DEVICE_TYPE_UNKNOWN:
		break;
	case NM_DEVICE_TYPE_ETHERNET:
		return CONNLINE_BEARER_ETHERNET;
	case NM_DEVICE_TYPE_WIFI:
		return CONNLINE_BEARER_WIFI;
	case NM_DEVICE_TYPE_BT:
		return CONNLINE_BEARER_BLUETOOTH;
	case NM_DEVICE_TYPE_WIMAX:
		return CONNLINE_BEARER_WIMAX;
	case NM_DEVICE_TYPE_MODEM:
		return CONNLINE_BEARER_CELLULAR;
	}

	return CONNLINE_BEARER_UNKNOWN;
}

static void nm_device_all_cb(DBusPendingCall *pending, void *user_data)
{
	struct connline_context *context = user_data;
	char ip[INET_ADDRSTRLEN+1];
	unsigned int dev_state;
	unsigned int dev_type;
	const char *interface;
	dbus_bool_t managed;
	DBusMessageIter arg;
	DBusMessage *reply;
	struct nm_dbus *nm;
	struct in_addr in4;
	char **properties;
	unsigned int ip4;

	if (dbus_pending_call_get_completed(pending) == FALSE)
		return;

	nm = context->backend_data;
	nm->call = NULL;

	reply = dbus_pending_call_steal_reply(pending);
	if (reply == NULL)
		goto error;

	if (dbus_message_iter_init(reply, &arg) == FALSE)
		goto error;

	if (connline_dbus_get_dict_entry_basic(&arg, "Managed",
					DBUS_TYPE_BOOLEAN, &managed) < 0)
		goto error;

	if (managed == FALSE)
		goto next;

	if (connline_dbus_get_dict_entry_basic(&arg, "State",
					DBUS_TYPE_UINT32, &dev_state) < 0)
		goto error;

	if (dev_state != NM_DEVICE_STATE_ACTIVATED)
		goto next;

	if (connline_dbus_get_dict_entry_basic(&arg, "DeviceType",
					DBUS_TYPE_UINT32, &dev_type) < 0)
		goto error;

	nm->bearer = nm_device_type_to_bearer(dev_type);
	if (!(context->bearer_type == CONNLINE_BEARER_UNKNOWN ||
					nm->bearer & context->bearer_type))
		goto next;

	__connline_call_connected_callback(context);

	free_devices(nm);

	/* If application does not want to get notified
	 * about the prpoerties, we will forget about it */
	if (context->property_callback == NULL)
		goto out;

	if (connline_dbus_get_dict_entry_basic(&arg, "Ip4Address",
						DBUS_TYPE_UINT32, &ip4) < 0)
		goto error;

	memset(ip, 0, INET_ADDRSTRLEN+1);

	in4.s_addr = ip4;

	if (inet_ntop(AF_INET, &in4, ip, INET_ADDRSTRLEN+1) == NULL)
		goto error;

	if (connline_dbus_get_dict_entry_basic(&arg, "IpInterface",
					DBUS_TYPE_STRING, &interface) < 0)
		goto error;

	properties = NULL;

	properties = insert_into_property_list(properties, "bearer",
					connline_bearer_to_string(nm->bearer));

	properties = insert_into_property_list(properties,
						"interface", interface);

	properties = insert_into_property_list(properties, "address", ip);

	if (properties != NULL)
		__connline_call_property_callback(context, properties);

	goto out;

next:
	nm->current_device++;

	if (nm->current_device < nm->nb_devices) {
		if (nm_device_get_all(context) < 0)
			goto error;
	} else {
		free_devices(nm);

		__connline_call_disconnected_callback(context);
	}

out:
	dbus_message_unref(reply);
	dbus_pending_call_unref(pending);

	return;

error:
	if (reply != NULL)
		dbus_message_unref(reply);

	dbus_pending_call_unref(pending);

	nm_backend_data_cleanup(context);
	__connline_call_error_callback(context, false);
}

static int nm_device_get_all(struct connline_context *context)
{
	struct nm_dbus *nm = context->backend_data;
	const char *dbus_if = NM_DBUS_NAME ".Device";
	DBusMessage *message = NULL;
	int ret = -EINVAL;
	char *device_path;

	if (nm->current_device >= nm->nb_devices || nm->devices == NULL)
		return 0;

	device_path = nm->devices[nm->current_device];

	message = dbus_message_new_method_call(NM_DBUS_NAME,
			device_path, DBUS_FREEDESKTOP_PROPERTIES, "GetAll");
	if (message == NULL)
		return -ENOMEM;

	if (dbus_message_append_args(message, DBUS_TYPE_STRING, &dbus_if,
						DBUS_TYPE_INVALID) == FALSE)
		goto out;

	if (dbus_connection_send_with_reply(context->dbus_cnx, message,
				&nm->call, DBUS_TIMEOUT_USE_DEFAULT) == FALSE)
		goto out;

	if (dbus_pending_call_set_notify(nm->call, nm_device_all_cb,
						context, NULL) == FALSE)
		goto out;

	ret = 0;

out:
	dbus_message_unref(message);

	return ret;
}

static void nm_devices_cb(DBusPendingCall *pending, void *user_data)
{
	struct connline_context *context = user_data;
	char **devices_obj = NULL;
	DBusMessageIter arg;
	DBusMessage *reply;
	struct nm_dbus *nm;
	int len;
	int i;

	if (dbus_pending_call_get_completed(pending) == FALSE)
		return;

	nm = context->backend_data;
	nm->call = NULL;

	reply = dbus_pending_call_steal_reply(pending);
	if (reply == NULL)
		goto error;

	if (dbus_message_iter_init(reply, &arg) == FALSE)
		goto error;

	if (connline_dbus_get_array(&arg, DBUS_TYPE_OBJECT_PATH,
						&len, &devices_obj) < 0)
		goto error;

	if (devices_obj != NULL && len > 0) {
		free_devices(nm);

		nm->nb_devices = len;
		nm->devices = calloc(nm->nb_devices, sizeof(char *));
		if (nm->devices == NULL)
			goto error;

		for (i = 0; i < len; i++) {
			nm->devices[i] = strdup(devices_obj[i]);
			if (nm->devices[i] == NULL) {
				nm->nb_devices = i;
				goto error;
			}
		}

		nm->current_device = 0;

		if (nm_device_get_all(context) < 0)
			goto error;

		free(devices_obj);
	}

	dbus_message_unref(reply);
	dbus_pending_call_unref(pending);

	return;

error:
	free(devices_obj);

	if (reply != NULL)
		dbus_message_unref(reply);

	dbus_pending_call_unref(pending);

	nm_backend_data_cleanup(context);
	__connline_call_error_callback(context, false);
}

static int nm_get_devices(struct connline_context *context)
{
	struct nm_dbus *nm = context->backend_data;
	DBusMessage *message = NULL;
	int ret = -EINVAL;

	message = dbus_message_new_method_call(NM_DBUS_NAME,
						NM_MANAGER_PATH,
						NM_DBUS_NAME,
						"GetDevices");
	if (message == NULL)
		return -ENOMEM;

	if (dbus_connection_send_with_reply(context->dbus_cnx, message,
				&nm->call, DBUS_TIMEOUT_USE_DEFAULT) == FALSE)
		goto out;

	if (dbus_pending_call_set_notify(nm->call, nm_devices_cb,
						context, NULL) == FALSE)
		goto out;

	ret = 0;

out:
	dbus_message_unref(message);

	return ret;
}

static dbus_bool_t is_connected(unsigned int state)
{
	if (state >= NM_STATE_CONNECTED_LOCAL)
		return TRUE;

	return FALSE;
}

static DBusHandlerResult watch_nm_state(DBusConnection *dbus_cnx,
						DBusMessage *message,
						void *user_data)
{
	struct connline_context *context = user_data;
	DBusMessageIter arg;
	struct nm_dbus *nm;
	const char *member;
	unsigned int state;

	if (dbus_message_get_type(message) != DBUS_MESSAGE_TYPE_SIGNAL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	member = dbus_message_get_member(message);
	if (member == NULL)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	if (strncmp(member, "StateChanged", sizeof("StateChanged")) != 0)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	nm = context->backend_data;

	if (dbus_message_iter_init(message, &arg) == FALSE)
		goto error;

	if (connline_dbus_get_basic(&arg, DBUS_TYPE_UINT32, &state) < 0)
		goto error;

	if (is_connected(state) == TRUE && state != nm->state) {
		if (nm->call != NULL) {
			dbus_pending_call_cancel(nm->call);
			dbus_pending_call_unref(nm->call);
			nm->call = NULL;
		}

		if (nm_get_devices(context) != 0)
			goto error;
	} else {
		if (is_connected(nm->state) == TRUE)
			__connline_call_disconnected_callback(context);
	}

	nm->state = state;

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

error:
	nm_backend_data_cleanup(context);
	__connline_call_error_callback(context, false);

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void nm_state_cb(DBusPendingCall *pending, void *user_data)
{
	struct connline_context *context = user_data;
	DBusMessage *reply = NULL;
	DBusMessageIter arg;
	struct nm_dbus *nm;
	unsigned int state;

	if (dbus_pending_call_get_completed(pending) == FALSE)
		return;

	nm = context->backend_data;
	nm->call = NULL;

	if (connline_dbus_setup_watch(context->dbus_cnx,
						NM_STATE_SIGNAL_MATCH_RULE,
						watch_nm_state, context) != 0)
		goto error;

	nm->state_watched = TRUE;

	reply = dbus_pending_call_steal_reply(pending);
	if (reply == NULL)
		goto error;

	if (dbus_message_iter_init(reply, &arg) == FALSE)
		goto error;

	if (connline_dbus_get_basic(&arg, DBUS_TYPE_UINT32, &state) != 0)
		goto error;

	if (is_connected(state) == TRUE) {
		if (nm_get_devices(context) != 0)
			goto error;
	} else
		__connline_call_disconnected_callback(context);

	nm->state = state;

	dbus_message_unref(reply);
	dbus_pending_call_unref(pending);

	return;

error:
	if (reply != NULL)
		dbus_message_unref(reply);

	dbus_pending_call_unref(pending);

	nm_backend_data_cleanup(context);
	__connline_call_error_callback(context, false);
}

static int nm_get_state(struct connline_context *context)
{
	struct nm_dbus *nm = context->backend_data;
	DBusMessage *message = NULL;
	int ret = -EINVAL;

	message = dbus_message_new_method_call(NM_DBUS_NAME,
						NM_MANAGER_PATH,
						NM_DBUS_NAME,
						"state");
	if (message == NULL)
		return -ENOMEM;

	if (dbus_connection_send_with_reply(context->dbus_cnx, message,
				&nm->call, DBUS_TIMEOUT_USE_DEFAULT) == FALSE)
		goto out;

	if (dbus_pending_call_set_notify(nm->call, nm_state_cb,
						context, NULL) == FALSE)
		goto out;

	ret = 0;

out:
	dbus_message_unref(message);

	return ret;
}

static DBusHandlerResult watch_nm_service(DBusConnection *dbus_cnx,
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

	if (strncmp(name, NM_DBUS_NAME, sizeof(NM_DBUS_NAME)) != 0)
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

	nm_backend_data_cleanup(context);
	connline_backend_unusable(context);

	return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static int nm_open(struct connline_context *context)
{
	struct nm_dbus *nm;

	if (context == NULL || context->dbus_cnx == NULL)
		return -EINVAL;

	if (connline_dbus_is_service_running(context->dbus_cnx,
						NM_DBUS_NAME) == FALSE)
		return -EINVAL;

	nm = context->backend_data;

	if (nm == NULL) {
		if (connline_dbus_setup_watch(context->dbus_cnx,
						NM_SERVICE_MATCH_RULE,
						watch_nm_service, context) < 0)
			return -ENOMEM;

		nm = calloc(1, sizeof(struct nm_dbus));
		if (nm == NULL)
			goto error;

		context->backend_data = nm;

		if (nm_get_state(context) < 0)
			goto error;
	}

	return 0;

error:
	nm_backend_data_cleanup(context);

	return -ENOMEM;
}

static int nm_close(struct connline_context *context)
{
	if (context == NULL || context->dbus_cnx == NULL)
		return -EINVAL;

	nm_backend_data_cleanup(context);

	return 0;
}

static enum connline_bearer nm_get_bearer(struct connline_context *context)
{
	struct nm_dbus *nm;

	if (context == NULL || context->dbus_cnx == NULL)
		return CONNLINE_BEARER_UNKNOWN;

	nm = context->backend_data;

	return nm->bearer;
}

static struct connline_backend_methods nm = {
	nm_open,
	nm_close,
	nm_get_bearer
};

struct connline_backend_methods *connline_plugin_setup_backend(DBusConnection *dbus_cnx)
{
	if (connline_dbus_is_service_running(dbus_cnx, NM_DBUS_NAME) == TRUE)
		return &nm;

	return NULL;
}
