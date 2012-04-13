/*
 *
 *  Connline library
 *
 *  Copyright (C) 2011-2012  Intel Corporation. All rights reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>

#include <glib.h>
#include <connline/connline.h>

void print_properties(char **properties)
{
	int i;

	if (properties == NULL)
		return;

	for (i = 0; properties[i] != NULL; i += 2) {
		char *property = properties[i];
		char *value = properties[i+1];

		printf("Property: %s = %s\n", property, value);
	}
}

gboolean cleanup_everything(gpointer data)
{
	GMainLoop *loop = data;

	connline_cleanup();

	g_main_loop_quit(loop);

	return FALSE;
}

void network_connection_callback(struct connline_context *context,
					enum connline_event event,
					char **properties,
					void *user_data)
{
	GMainLoop *loop = user_data;

	switch (event) {
	case CONNLINE_EVENT_ERROR:
		printf("Context became invalid\n");

		connline_close(context);

		g_timeout_add(0, cleanup_everything, loop);

		break;
	case CONNLINE_EVENT_DISCONNECTED:
		printf("We are not connected.\n");
		break;
	case CONNLINE_EVENT_CONNECTED:
		printf("We are connected (bearer: %u)!\n",
				connline_get_bearer(context));
		break;
	case CONNLINE_EVENT_PROPERTY:
		print_properties(properties);
		g_strfreev(properties);

		break;
	default:
		break;
	}
}

int main( void )
{
	struct connline_context *cnx = NULL;
	GMainLoop *loop = NULL;

	loop = g_main_loop_new(NULL, FALSE);

	if (connline_init(CONNLINE_EVENT_LOOP_GLIB, NULL) != 0)
		goto error;

	cnx = connline_new(CONNLINE_BEARER_ETHERNET);
	if (cnx == NULL)
		goto error;

	connline_set_error_callback(cnx, network_connection_callback);
	connline_set_disconnected_callback(cnx, network_connection_callback);
	connline_set_connected_callback(cnx, network_connection_callback);
	connline_set_property_callback(cnx, network_connection_callback);
	connline_set_user_data(cnx, loop);

	if (connline_open(cnx, FALSE) != 0)
		goto error;

	g_main_loop_run(loop);

	g_main_loop_unref(loop);

	return EXIT_SUCCESS;

error:
	printf("An error occured... exiting.\n");

	connline_close(cnx);
	connline_cleanup();

	if (loop != NULL)
		g_main_loop_unref(loop);

	return EXIT_FAILURE;
}

