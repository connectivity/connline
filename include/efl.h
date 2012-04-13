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

#ifndef __EFL_H__
#define __EFL_H__

#include <connline/private.h>

DBusConnection *__connline_efl_setup_dbus_event_loop(void);

int __connline_efl_trigger_callback(struct connline_context *context,
						connline_callback_f callback,
						enum connline_event event,
						char **changed_property);

void __connline_efl_trigger_cleanup(struct connline_context *context);

void __connline_efl_cleanup(DBusConnection *dbus_cnx);


#endif

