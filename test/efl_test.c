/*
 *
 *  Connline library
 *
 *  Copyright (C) 2011-2013  Intel Corporation. All rights reserved.
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

#include <connline/connline.h>
#include <Ecore.h>

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

Eina_Bool cleanup_everything(void *data)
{
	connline_cleanup();

	ecore_main_loop_quit();

	return EINA_FALSE;
}

void network_connection_callback(struct connline_context *context,
					enum connline_event event,
					char **properties,
					void *user_data)
{
	char **prop;

	switch (event) {
	case CONNLINE_EVENT_ERROR:
		printf("Context became invalid\n");

		connline_close(context);

		ecore_timer_add(0, cleanup_everything, NULL);

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

		for (prop = properties; *prop != NULL; prop++)
			free(*prop);
		free(properties);
		break;
	default:
		break;
	}
}

int main( void )
{
	struct connline_context *cnx = NULL;

	ecore_init();

	if (connline_init(CONNLINE_EVENT_LOOP_EFL, NULL) != 0)
		goto error;

	cnx = connline_new(CONNLINE_BEARER_ETHERNET);
	if (cnx == NULL)
		goto error;

	connline_set_error_callback(cnx, network_connection_callback);
	connline_set_disconnected_callback(cnx, network_connection_callback);
	connline_set_connected_callback(cnx, network_connection_callback);
	connline_set_property_callback(cnx, network_connection_callback);

	if (connline_open(cnx, EINA_FALSE) != 0)
		goto error;

	ecore_main_loop_begin();
	ecore_shutdown();

	return EXIT_SUCCESS;

error:
	printf("An error occured... exiting.\n");

	connline_close(cnx);
	connline_cleanup();

	ecore_shutdown();

	return EXIT_FAILURE;
}
