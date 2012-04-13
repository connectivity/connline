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

#include <connline/private.h>

#ifdef CONNLINE_EVENT_GLIB
#include <connline/glib_loop.h>
#endif

#ifdef CONNLINE_EVENT_EFL
#include <connline/efl.h>
#endif

#ifdef CONNLINE_EVENT_LIBEVENT
#include <connline/libevent.h>
#endif

static enum connline_event_loop loop_type = CONNLINE_EVENT_LOOP_UNKNOWN;

int __connline_setup_event_loop(enum connline_event_loop event_loop_type)
{
	if (loop_type != CONNLINE_EVENT_LOOP_UNKNOWN)
		return 0;

	switch (event_loop_type)
	{
#ifdef CONNLINE_EVENT_GLIB
	case CONNLINE_EVENT_LOOP_GLIB:
		break;
#endif
#ifdef CONNLINE_EVENT_EFL
	case CONNLINE_EVENT_LOOP_EFL:
		break;
#endif
#ifdef CONNLINE_EVENT_LIBEVENT
	case CONNLINE_EVENT_LOOP_LIBEVENT:
		break;
#endif
	case CONNLINE_EVENT_LOOP_UNKNOWN:
		/* Falling over */
	default:
		loop_type = CONNLINE_EVENT_LOOP_UNKNOWN;
		return -EINVAL;
	}

	loop_type = event_loop_type;

	return 0;
}

DBusConnection *__connline_setup_dbus_event_loop(void *data)
{
	DBusConnection *dbus_cnx = NULL;

	switch (loop_type) {
#ifdef CONNLINE_EVENT_GLIB
	case CONNLINE_EVENT_LOOP_GLIB:
		dbus_cnx = __connline_glib_setup_dbus_event_loop();
		break;
#endif
#ifdef CONNLINE_EVENT_EFL
	case CONNLINE_EVENT_LOOP_EFL:
		dbus_cnx = __connline_efl_setup_dbus_event_loop();
		break;
#endif
#ifdef CONNLINE_EVENT_LIBEVENT
	case CONNLINE_EVENT_LOOP_LIBEVENT:
		dbus_cnx = __connline_libevent_setup_dbus_event_loop(data);
#endif
	case CONNLINE_EVENT_LOOP_UNKNOWN:
		/* Falling over */
	default:
		break;
	}

	return dbus_cnx;
}

int __connline_trigger_callback(struct connline_context *context,
					connline_callback_f callback,
					enum connline_event event,
					char **changed_property)
{
	switch (loop_type) {
#ifdef CONNLINE_EVENT_GLIB
	case CONNLINE_EVENT_LOOP_GLIB:
		return __connline_glib_trigger_callback(context,
					callback, event, changed_property);
#endif
#ifdef CONNLINE_EVENT_EFL
	case CONNLINE_EVENT_LOOP_EFL:
		return __connline_efl_trigger_callback(context,
					callback, event, changed_property);
#endif
#ifdef CONNLINE_EVENT_LIBEVENT
	case CONNLINE_EVENT_LOOP_LIBEVENT:
		return __connline_libevent_trigger_callback(context,
					callback, event, changed_property);
#endif
	case CONNLINE_EVENT_LOOP_UNKNOWN:
		/* Falling over */
	default:
		break;
	}

	return -EINVAL;
}

void __connline_trigger_cleanup(struct connline_context *context)
{
	switch (loop_type) {
#ifdef CONNLINE_EVENT_GLIB
	case CONNLINE_EVENT_LOOP_GLIB:
		__connline_glib_trigger_cleanup(context);
		break;
#endif
#ifdef CONNLINE_EVENT_EFL
	case CONNLINE_EVENT_LOOP_EFL:
		__connline_efl_trigger_cleanup(context);
		break;
#endif
#ifdef CONNLINE_EVENT_LIBEVENT
	case CONNLINE_EVENT_LOOP_LIBEVENT:
		__connline_libevent_trigger_cleanup(context);
		break;
#endif
	default:
		break;
	}
}

void __connline_cleanup_event_loop(DBusConnection *dbus_cnx)
{
	switch (loop_type) {
#ifdef CONNLINE_EVENT_GLIB
	case CONNLINE_EVENT_LOOP_GLIB:
		__connline_glib_cleanup(dbus_cnx);
		break;
#endif
#ifdef CONNLINE_EVENT_EFL
	case CONNLINE_EVENT_LOOP_EFL:
		__connline_efl_cleanup(dbus_cnx);
		break;
#endif
#ifdef CONNLINE_EVENT_LIBEVENT
	case CONNLINE_EVENT_LOOP_LIBEVENT:
		__connline_libevent_cleanup(dbus_cnx);
		break;
#endif
	case CONNLINE_EVENT_LOOP_UNKNOWN:
		/* Falling over */
	default:
		break;
	}

	loop_type = CONNLINE_EVENT_LOOP_UNKNOWN;
}

