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

static int populate_backend_plugin(void *handle,
				struct connline_backend_plugin *backend)
{
	const char **pointer;

	backend->setup = dlsym(handle, "connline_plugin_setup_backend");
	pointer = dlsym(handle, "connline_backend_watch_rule");
	backend->watch_rule = *pointer;
	pointer = dlsym(handle, "connline_backend_service_name");
	backend->service_name = *pointer;

	if (backend->setup == NULL || backend->watch_rule == NULL ||
						backend->service_name == NULL)
		return -1;

	return 0;
}

static int connline_load_plugin(enum connline_event_loop event_loop_type,
			struct connline_event_loop_plugin **event_plugin)
{
	char filename[FILENAME_SIZE];
	struct dirent *dir_ent;
	const char *prefix;
	bool found = false;
	void *handle;
	int ret = 0;
	DIR *dir;

	if (event_loop_type == CONNLINE_EVENT_LOOP_UNKNOWN &&
							event_plugin == NULL)
		prefix = "backend_";
	else
		prefix = "event_";

	dir = opendir(CONNLINE_PLUGIN_DIR);
	if (dir == NULL)
		return -ENOENT;

	while ((dir_ent = readdir(dir)) != NULL) {
		if (strstr(dir_ent->d_name, ".so") == NULL)
			continue;

		if (strstr(dir_ent->d_name, prefix) != dir_ent->d_name)
			continue;

		memset(filename, 0, FILENAME_SIZE);
		snprintf(filename, FILENAME_SIZE, "%s/%s",
				CONNLINE_PLUGIN_DIR, dir_ent->d_name);

		handle = dlopen(filename, RTLD_NOW);
		if (handle == NULL)
			continue;

		if (event_loop_type != CONNLINE_EVENT_LOOP_UNKNOWN) {
			unsigned int *plugin_event_loop;

			*event_plugin = calloc(
				sizeof(struct connline_event_loop_plugin), 1);
			if (*event_plugin == NULL) {
				ret = -ENOMEM;
				goto loop_or_error;
			}

			plugin_event_loop = dlsym(handle,
					"connline_plugin_event_loop_type");
			if (plugin_event_loop == NULL ||
					*plugin_event_loop != event_loop_type)
				goto loop_or_error;

			if (populate_event_plugin(handle,
							*event_plugin) != 0) {
				free(*event_plugin);
				goto loop_or_error;
			}

			(*event_plugin)->handle = handle;
			found = true;

			break;
		} else {
			struct connline_backend_plugin *backend_plugin;

			backend_plugin = calloc(
				sizeof(struct connline_backend_plugin), 1);
			if (backend_plugin == NULL) {
				ret = -ENOMEM;
				goto loop_or_error;
			}

			if (populate_backend_plugin(handle,
							backend_plugin) != 0) {
				free(backend_plugin);
				goto loop_or_error;
			}

			backend_plugin->handle = handle;
			ret = __connline_backend_add(backend_plugin);
			if (ret < 0) {
				free(backend_plugin);
				goto loop_or_error;
			}

			found = true;
		}

loop_or_error:
		if (ret < 0) {
			dlclose(handle);
			break;
		} else if (event_loop_type != CONNLINE_EVENT_LOOP_UNKNOWN)
			dlclose(handle);
	}

	closedir(dir);

	if (ret == 0 && found == false)
		return -ENOENT;

	return ret;
}

struct connline_event_loop_plugin *
__connline_load_event_loop_plugin(enum connline_event_loop event_loop_type)
{
	struct connline_event_loop_plugin *event_plugin;

	if (connline_load_plugin(event_loop_type, &event_plugin) != 0)
		event_plugin = NULL;

	return event_plugin;
}

int __connline_load_backend_plugins(void)
{
	return connline_load_plugin(CONNLINE_EVENT_LOOP_UNKNOWN, NULL);
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
