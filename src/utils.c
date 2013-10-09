/*
 *  Connline library
 *
 *  Copyright (C) 2011-2013  Intel Corporation. All rights reserved.
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

#include <connline/utils.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <ctype.h>

char *get_processus_name(void)
{
	char cmdline_path[20];
	FILE *cmdline_file;
	char cmdline[512];
	char *proc_name;
	pid_t pid;
	char *c;

	proc_name = NULL;

	pid = getpid();

	if (snprintf(cmdline_path, 20, "/proc/%u/cmdline", pid) < 0)
		return NULL;

	cmdline_file = fopen(cmdline_path, "r");
	if (cmdline_file == NULL)
		return NULL;

	memset(cmdline, 0, 512);
	if (fread(cmdline, 1, 512, cmdline_file) == 0)
		goto error;

	c = strchr(cmdline, ' ');
	if (c != NULL)
		*c = '\0';

	c = strrchr(cmdline, '/');
	if (c == NULL)
		c = cmdline;
	else
		c++;

	proc_name = calloc(strlen(c) + 1, sizeof(char));
	if (proc_name == NULL)
		goto error;

	strcpy(proc_name, c);

	for (c = proc_name; *c != '\0'; c++)
		if (isalnum(*c) == 0)
			*c = '_';

error:
	fclose(cmdline_file);

	return proc_name;
}

char *get_new_unique_name(void)
{
	char *session_name = NULL;
	char *unknown = "unknown";
	time_t timestamp;
	char *proc_name;
	int rand_num;
	int length;

	timestamp = time(NULL);
	rand_num = rand();

	proc_name = get_processus_name();
	if (proc_name == NULL)
		proc_name = unknown;

	length = strlen(proc_name);
	length += 28; /* '_' + pid + '_' + timestamp + '_' + rand '\0' */

	session_name = calloc(length, sizeof(char));
	if (session_name == NULL)
		goto error;

	if (snprintf(session_name, length, "%s_%u_%lu_%d",
				proc_name, getpid(), timestamp, rand_num) < 0)
		goto error;

	if (proc_name != unknown)
		free(proc_name);

	return session_name;

error:
	free(session_name);
	return NULL;
}

char **insert_into_property_list(char **properties,
					const char *name,
					const char *value)
{
	char **new_list = properties;
	char *new_value = NULL;
	char *new_name = NULL;
	int length = 0;
	int app = 0;
	int n;

	if (name == NULL || value == NULL || value[0] == '\0')
		return properties;

	n = strlen(name);

	if (properties != NULL) {
		for (length = 0; properties[length] != NULL; length += 2);

		for (app = 0; properties[app] != NULL &&
			strncmp(properties[app], name, n) != 0; app += 2);
	}

	if (app >= length) {
		new_name = calloc(n + 1, sizeof(char));
		if (new_name == NULL)
			return properties;

		strncpy(new_name, name, n);
	}

	n = strlen(value) + 1;

	if (app < length) {
		app++;
		n += strlen(properties[app]) + 1;

		new_value = calloc(n, sizeof(char));
		if (new_value == NULL)
			goto error;

		if (snprintf(new_value, n, "%s,%s",
					properties[app], value) < 0)
			goto error;

		length = app + 2;
	} else {
		length += 2;

		new_list = realloc(properties, sizeof(char *) * (length + 1));
		if (new_list == NULL)
			goto error;

		properties = new_list;

		new_list[length - 2] = NULL;
		new_list[length - 1] = NULL;
		new_list[length] = NULL;

		new_value = calloc(n, sizeof(char));
		if (new_value == NULL)
			goto error;

		strncpy(new_value, value, n);
	}

	if (new_name != NULL)
		new_list[length - 2] = new_name;

	new_list[length - 1] = new_value;

	return new_list;

error:
	if (new_name != NULL)
		free(new_name);

	if (new_value != NULL)
		free(new_value);

	return properties;
}

void property_list_free(char **properties)
{
	int i;

	if (properties == NULL)
		return;

	for (i = 0; properties[i] != NULL; i++)
		free(properties[i]);

	free(properties);
}

const char *connline_bearer_to_string(enum connline_bearer bearer)
{
	switch (bearer) {
	case CONNLINE_BEARER_ETHERNET:
		return "ethernet";
	case CONNLINE_BEARER_WIFI:
		return "wifi";
	case CONNLINE_BEARER_CELLULAR:
		return "cellular";
	case CONNLINE_BEARER_WIMAX:
		return "wimax";
	case CONNLINE_BEARER_BLUETOOTH:
		return "bluetooth";
	case CONNLINE_BEARER_USB:
		return "usb";
	case CONNLINE_BEARER_UNKNOWN:
	default:
		break;
	}

	return "*";
}
