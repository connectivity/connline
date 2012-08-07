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

#ifndef __CONNLINE_BACKEND_H__
#define __CONNLINE_BACKEND_H__

#include <connline/data.h>

typedef int (*__connline_open_f) (struct connline_context *);
typedef int (*__connline_close_f) (struct connline_context *);
typedef enum connline_bearer (*__connline_get_bearer_f) (struct connline_context *);

struct connline_backend_methods {
	__connline_open_f __connline_open;
	__connline_close_f __connline_close;
	__connline_get_bearer_f __connline_get_bearer;
};

struct connline_backend_methods *__connline_setup_backend(DBusConnection *dbus_cnx);

#endif

