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

#ifndef __CONNLINE_EVENT_H__
#define __CONNLINE_EVENT_H__

#include <connline/data.h>

typedef DBusConnection *(*__connline_setup_event_loop_f) (void *);
typedef int (*__connline_trigger_callback_f) (struct connline_context *,
						connline_callback_f,
						enum connline_event,
						char **);
typedef void (*__connline_trigger_cleanup_f) (struct connline_context *);
typedef void (*__connline_cleanup_event_loop_f) (DBusConnection *);


int __connline_trigger_callback(struct connline_context *context,
					connline_callback_f callback,
					enum connline_event event,
					char **changed_property);

void __connline_trigger_cleanup(struct connline_context *context);

static inline
void __connline_call_error_callback(struct connline_context *context,
							bool no_backend)
{
	if (context->event_callback != NULL) {
		__connline_trigger_cleanup(context);

		__connline_trigger_callback(context,
					context->event_callback,
					no_backend == true ?
					CONNLINE_EVENT_NO_BACKEND :
					CONNLINE_EVENT_ERROR, NULL);
	}
}

static inline
void __connline_call_disconnected_callback(struct connline_context *context)
{
	if (context->event_callback != NULL)
		__connline_trigger_callback(context,
					context->event_callback,
					CONNLINE_EVENT_DISCONNECTED, NULL);
}

static inline
void __connline_call_connected_callback(struct connline_context *context)
{
	if (context->event_callback != NULL)
		__connline_trigger_callback(context,
					context->event_callback,
					CONNLINE_EVENT_CONNECTED, NULL);
}

static inline
void __connline_call_property_callback(struct connline_context *context,
							char **property_values)
{
	if (context->property_callback != NULL)
		__connline_trigger_callback(context,
					context->property_callback,
					CONNLINE_EVENT_PROPERTY,
					property_values);
}

#endif

