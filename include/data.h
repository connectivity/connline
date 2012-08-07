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

#ifndef __DATA_H__
#define __DATA_H__

#include <connline/connline.h>

#include <stdbool.h>
#include <errno.h>
#include <dbus/dbus.h>

struct connline_context {
	DBusConnection *dbus_cnx;

	unsigned int bearer_type;
	bool background_connection;

	bool is_online;

	connline_callback_f error_callback;
	connline_callback_f disconnected_callback;
	connline_callback_f connected_callback;
	connline_callback_f property_callback;

	void *user_data;

	void *backend_data;
};

#endif

