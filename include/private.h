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

#ifndef __CONNLINE_PRIVATE_H__
#define __CONNLINE_PRIVATE_H__

#include <config.h>

#include <connline/event.h>
#include <connline/list.h>

struct connline_event_loop_plugin {
	void *handle;

	__connline_setup_event_loop_f setup_event_loop;
	__connline_trigger_callback_f trigger_callback;
	__connline_trigger_cleanup_f trigger_cleanup;
	__connline_cleanup_event_loop_f cleanup_event_loop;
};

int __connline_setup_event_loop(enum connline_event_loop event_loop_type);

DBusConnection *__connline_setup_dbus_event_loop(void *data);

void __connline_cleanup_event_loop(DBusConnection *dbus_cnx);

#include <connline/backend.h>

struct connline_backend_plugin {
	void *handle;

	const char *service_name;
	const char *watch_rule;
	__connline_setup_backend_f setup;
};

int __connline_setup_backend(DBusConnection *dbus_cnx);

int __connline_backend_add(struct connline_backend_plugin *backend_plugin);

void __connline_close(struct connline_context *context);

void __connline_disconnect_contexts(void);

void __connline_reconnect_contexts(void);

void __connline_invalidate_contexts(void);

void __connline_cleanup_backend(void);

struct connline_event_loop_plugin *
__connline_load_event_loop_plugin(enum connline_event_loop event_loop_type);

int __connline_load_backend_plugins(void);

void __connline_cleanup_event_plugin(struct connline_event_loop_plugin *event_plugin);

void __connline_cleanup_backend_plugin(struct connline_backend_plugin *backend_plugin);

#endif
