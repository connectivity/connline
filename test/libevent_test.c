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
#include <string.h>

#include <connline/connline.h>
#include <event2/event.h>

void print_properties(const char **properties)
{
	int i;

	if (properties == NULL)
		return;

	for (i = 0; properties[i] != NULL; i += 2) {
		const char *property = properties[i];
		const char *value = properties[i+1];

		printf("Property: %s = %s\n", property, value);
	}
}

void cleanup_everything(int fd, short event, void *arg)
{
	struct event_base *ev_base = arg;

	connline_cleanup();

	event_base_loopbreak(ev_base);
}

void network_connection_callback(struct connline_context *context,
					enum connline_event event,
					const char **properties,
					void *user_data)
{
	struct event_base *ev_base = user_data;
	struct timeval timeout;
	struct event *ev;

	switch (event) {
	case CONNLINE_EVENT_ERROR:
		printf("Context became invalid\n");

		connline_close(context);

		ev = evtimer_new(ev_base, cleanup_everything, ev_base);

		memset(&timeout, 0, sizeof(struct timeval));

		evtimer_add(ev, &timeout);

		break;
	case CONNLINE_EVENT_NO_BACKEND:
		printf("No Connection backend\n");
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
		break;
	default:
		break;
	}
}

int main( void )
{
	struct connline_context *cnx = NULL;
	struct event_base *ev_base = NULL;

	ev_base = event_base_new();

	if (connline_init(CONNLINE_EVENT_LOOP_LIBEVENT, ev_base) != 0)
		goto error;

	cnx = connline_new(CONNLINE_BEARER_ETHERNET);
	if (cnx == NULL)
		goto error;

	connline_set_event_callback(cnx, network_connection_callback);
	connline_set_property_callback(cnx, network_connection_callback);
	connline_set_user_data(cnx, ev_base);

	if (connline_open(cnx, false) != 0)
		goto error;

	event_base_dispatch(ev_base);

	event_base_free(ev_base);

	return EXIT_SUCCESS;

error:
	printf("An error occured... exiting.\n");

	connline_close(cnx);
	connline_cleanup();

	if (ev_base != NULL)
		event_base_free(ev_base);

	return EXIT_FAILURE;
}

