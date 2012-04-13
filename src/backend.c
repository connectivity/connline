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

#ifdef CONNLINE_BACKEND_CONNMAN
#include <connline/connman.h>
#endif

#ifdef CONNLINE_BACKEND_NM
#include <connline/nm.h>
#endif

#ifdef CONNLINE_BACKEND_WICD
#include <connline/wicd.h>
#endif

#include <stdlib.h>

struct connline_backend_methods *__connline_setup_backend(DBusConnection *dbus_cnx)
{
	struct connline_backend_methods *backend = NULL;

#ifdef CONNLINE_BACKEND_CONNMAN
	backend = __connline_detect_connman(dbus_cnx);
#endif

#ifdef CONNLINE_BACKEND_NM
	if (backend == NULL)
		backend = __connline_detect_nm(dbus_cnx);
#endif

#ifdef CONNLINE_BACKEND_WICD
	if (backend == NULL)
		backend = __connline_detect_wicd(dbus_cnx);
#endif

	return backend;
}

