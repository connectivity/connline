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
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>
#include <sys/signalfd.h>

#include <glib.h>
#include <connline/connline.h>

static gboolean signal_handler(GIOChannel *channel,
					GIOCondition cond,
					gpointer user_data)
{
	GMainLoop *loop = user_data;
	struct signalfd_siginfo si;
	ssize_t result;
	int fd;

	if (cond & (G_IO_NVAL | G_IO_ERR | G_IO_HUP))
		return FALSE;

	printf("Terminating...\n");

	fd = g_io_channel_unix_get_fd(channel);

	result = read(fd, &si, sizeof(si));
	if (result != sizeof(si))
		return FALSE;

	switch (si.ssi_signo) {
	case SIGINT:
	case SIGTERM:
		g_main_loop_quit(loop);
		break;
	}

	return TRUE;
}

static unsigned int setup_signal_handler(GMainLoop *loop)
{
	GIOChannel *channel;
	unsigned int source;
	sigset_t mask;
	int fd;

	sigemptyset(&mask);
	sigaddset(&mask, SIGINT);
	sigaddset(&mask, SIGTERM);

	if (sigprocmask(SIG_BLOCK, &mask, NULL) < 0)
		return -1;

	fd = signalfd(-1, &mask, 0);
	if (fd < 0)
		return -1;

	channel = g_io_channel_unix_new(fd);

	g_io_channel_set_close_on_unref(channel, TRUE);
	g_io_channel_set_encoding(channel, NULL, NULL);
	g_io_channel_set_buffered(channel, FALSE);

	source = g_io_add_watch(channel,
				G_IO_IN | G_IO_HUP | G_IO_ERR | G_IO_NVAL,
				signal_handler, loop);

	g_io_channel_unref(channel);

	return source;
}

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

void network_connection_callback(struct connline_context *context,
					enum connline_event event,
					const char **properties,
					void *user_data)
{
	GMainLoop *loop = user_data;

	switch (event) {
	case CONNLINE_EVENT_ERROR:
		printf("Context became invalid\n");
		g_main_loop_quit(loop);

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
	int err = EXIT_SUCCESS;
	GMainLoop *loop = NULL;
	int sig_hdl = -1;

	loop = g_main_loop_new(NULL, FALSE);

	if (connline_init(CONNLINE_EVENT_LOOP_GLIB, NULL) != 0)
		goto error;

	cnx = connline_open(CONNLINE_BEARER_UNKNOWN, false,
				network_connection_callback, loop);
	if (cnx == NULL)
		goto error;

	sig_hdl = setup_signal_handler(loop);
	if (sig_hdl < 0)
		goto error;

	g_main_loop_run(loop);

	goto done;

error:
	printf("An error occured... exiting.\n");
	err = EXIT_FAILURE;

done:
	if (sig_hdl >= 0)
		g_source_remove(sig_hdl);

	connline_close(cnx);
	connline_cleanup();

	if (loop != NULL)
		g_main_loop_unref(loop);

	return err;
}

