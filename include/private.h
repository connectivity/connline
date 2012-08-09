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

#include <connline/event.h>

struct connline_event_loop_plugin {
	void *handle;

	__connline_setup_event_loop_f setup_event_loop;
	__connline_trigger_callback_f trigger_callback;
	__connline_trigger_cleanup_f trigger_cleanup;
	__connline_cleanup_event_loop_f cleanup_event_loop;
};

#include <connline/backend.h>

struct connline_backend_plugin {
	void *handle;

	struct connline_backend_methods *methods;
};

struct connline_event_loop_plugin *
__connline_load_event_loop_plugin(enum connline_event_loop event_loop_type);

struct connline_backend_plugin *
__connline_load_backend_plugin(DBusConnection *dbus_cnx);

void __connline_cleanup_event_plugin(struct connline_event_loop_plugin *event_plugin);

void __connline_cleanup_backend_plugin(struct connline_backend_plugin *backend_plugin);

#endif