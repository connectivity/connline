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

#include <connline/backend.h>
#include <connline/private.h>

#include <stdlib.h>

static struct connline_backend_plugin *backend = NULL;

struct connline_backend_methods *__connline_setup_backend(DBusConnection *dbus_cnx)
{
	if (dbus_cnx == NULL)
		return NULL;

	backend = __connline_load_backend_plugin(dbus_cnx);
	if (backend == NULL)
		return NULL;

	return backend->methods;
}

void __connline_cleanup_backend(void)
{
	if (backend == NULL)
		return;

	__connline_cleanup_backend_plugin(backend);

	backend = NULL;
}
