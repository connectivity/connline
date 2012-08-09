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

#include <connline/connline.h>
#include <connline/private.h>

#include <dirent.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define FILENAME_SIZE sizeof(CONNLINE_PLUGIN_DIR) + NAME_MAX

static int populate_event_plugin(void *handle,
			struct connline_event_loop_plugin *event_plugin)
{
	event_plugin->setup_event_loop = dlsym(handle,
					"connline_plugin_setup_event_loop");
	event_plugin->trigger_callback = dlsym(handle,
					"connline_plugin_trigger_callback");
	event_plugin->trigger_cleanup = dlsym(handle,
					"connline_plugin_trigger_cleanup");
	event_plugin->cleanup_event_loop = dlsym(handle,
					"connline_plugin_cleanup_event_loop");

	if (event_plugin->setup_event_loop == NULL ||
				event_plugin->trigger_callback == NULL ||
				event_plugin->trigger_cleanup == NULL ||
				event_plugin->cleanup_event_loop == NULL)
		return -1;

	return 0;
}

static void *connline_load_plugin(enum connline_event_loop event_loop_type,
						void *connline_plugin,
						DBusConnection *dbus_cnx)
{
	char filename[FILENAME_SIZE];
	struct dirent *dir_ent;
	void *plugin = NULL;
	void *handle;
	DIR *dir;

	if (connline_plugin == NULL)
		return NULL;

	if (event_loop_type == CONNLINE_EVENT_LOOP_UNKNOWN && dbus_cnx == NULL)
		return NULL;

	dir = opendir(CONNLINE_PLUGIN_DIR);
	if (dir == NULL)
		return NULL;

	while ((dir_ent = readdir(dir)) != NULL) {
		if (strstr(dir_ent->d_name, ".so") == NULL)
			continue;

		memset(filename, 0, FILENAME_SIZE);
		snprintf(filename, FILENAME_SIZE, "%s/%s",
				CONNLINE_PLUGIN_DIR, dir_ent->d_name);

		handle = dlopen(filename, RTLD_NOW);
		if (handle == NULL)
			continue;

		if (event_loop_type != CONNLINE_EVENT_LOOP_UNKNOWN) {
			struct connline_event_loop_plugin *event_plugin =
							connline_plugin;
			unsigned int *plugin_event_loop;

			plugin_event_loop = dlsym(handle,
					"connline_plugin_event_loop_type");
			if (plugin_event_loop == NULL)
				goto loop_again;

			if (*plugin_event_loop != event_loop_type)
				goto loop_again;

			if (populate_event_plugin(handle, event_plugin) != 0)
				goto loop_again;

			event_plugin->handle = handle;

			plugin = event_plugin;
			break;
		} else {
			struct connline_backend_plugin *backend_plugin =
							connline_plugin;
			__connline_setup_backend_f setup_backend;

			setup_backend = dlsym(handle,
					"connline_plugin_setup_backend");
			if (setup_backend == NULL)
				goto loop_again;

			backend_plugin->methods = setup_backend(dbus_cnx);
			if (backend_plugin->methods == NULL)
				goto loop_again;

			backend_plugin->handle = handle;

			plugin = backend_plugin;
			break;
		}
loop_again:
		dlclose(handle);
		continue;
	}

	closedir(dir);

	return plugin;
}

struct connline_event_loop_plugin *
__connline_load_event_loop_plugin(enum connline_event_loop event_loop_type)
{
	struct connline_event_loop_plugin *event_plugin;

	event_plugin = calloc(sizeof(struct connline_event_loop_plugin), 1);
	if (event_plugin == NULL)
		return NULL;

	if (connline_load_plugin(event_loop_type,
					event_plugin, NULL) == NULL) {
		free(event_plugin);
		return NULL;
	}

	return event_plugin;
}

struct connline_backend_plugin *
__connline_load_backend_plugin(DBusConnection *dbus_cnx)
{
	struct connline_backend_plugin *backend_plugin;

	backend_plugin = calloc(sizeof(struct connline_backend_plugin), 1);
	if (backend_plugin == NULL)
		return NULL;

	if (connline_load_plugin(CONNLINE_EVENT_LOOP_UNKNOWN,
					backend_plugin, dbus_cnx) == NULL) {
		free(backend_plugin);
		return NULL;
	}

	return backend_plugin;
}

void __connline_cleanup_event_plugin(struct connline_event_loop_plugin *event_plugin)
{
	dlclose(event_plugin->handle);

	free(event_plugin);
}

void __connline_cleanup_backend_plugin(struct connline_backend_plugin *backend_plugin)
{
	dlclose(backend_plugin->handle);

	free(backend_plugin);
}
