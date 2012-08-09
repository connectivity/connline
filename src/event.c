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

#include <config.h>

#include <connline/event.h>
#include <connline/private.h>

static struct connline_event_loop_plugin *event_loop = NULL;

int __connline_setup_event_loop(enum connline_event_loop event_loop_type)
{
	if (event_loop_type == CONNLINE_EVENT_LOOP_UNKNOWN)
		return 0;

	event_loop = __connline_load_event_loop_plugin(event_loop_type);
	if (event_loop == NULL)
		return -EINVAL;

	return 0;
}

DBusConnection *__connline_setup_dbus_event_loop(void *data)
{
	if (event_loop == NULL)
		return NULL;

	return event_loop->setup_event_loop(data);
}

int __connline_trigger_callback(struct connline_context *context,
					connline_callback_f callback,
					enum connline_event event,
					char **changed_property)
{
	if (event_loop == NULL)
		return -EINVAL;

	return event_loop->trigger_callback(context,
					callback, event, changed_property);
}

void __connline_trigger_cleanup(struct connline_context *context)
{
	if (event_loop == NULL)
		return;

	event_loop->trigger_cleanup(context);
}

void __connline_cleanup_event_loop(DBusConnection *dbus_cnx)
{
	if (event_loop == NULL)
		return;

	event_loop->cleanup_event_loop(dbus_cnx);

	__connline_cleanup_event_plugin(event_loop);

	event_loop = NULL;
}

