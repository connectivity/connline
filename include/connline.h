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

#ifndef __CONNLINE_H__
#define __CONNLINE_H__

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * Event loop type enumeration
 * This  is  used  when  initializing  Connline, so  it sets  up  according  to
 * application's event loop.
 */
enum connline_event_loop {
	CONNLINE_EVENT_LOOP_UNKNOWN  = 0,
	CONNLINE_EVENT_LOOP_GLIB     = 1,
	CONNLINE_EVENT_LOOP_EFL      = 2,
	CONNLINE_EVENT_LOOP_LIBEVENT = 3,
};

/**
 * Connection Bearer type enumeration
 * Used when  creating  a  connline  context  to  select  against which  bearer
 * the context will work.   Unknown  means  any.  For  instance  if  bearer  is
 * set to  wifi and  if the connection daemon gets  connected through  ethernet
 * the context will not be connected.
 */
enum connline_bearer {
	CONNLINE_BEARER_UNKNOWN   = 1 << 0,
	CONNLINE_BEARER_ETHERNET  = 1 << 1,
	CONNLINE_BEARER_WIFI      = 1 << 2,
	CONNLINE_BEARER_CELLULAR  = 1 << 3,
	CONNLINE_BEARER_WIMAX     = 1 << 4,
	CONNLINE_BEARER_BLUETOOTH = 1 << 5,
	CONNLINE_BEARER_USB       = 1 << 6,
};

/**
 * Connline Event type enumeration
 * As   an  asynchronous  library,  connline  will  generate   certain  events.
 * CONNLINE_EVENT_ERROR: when memory allocation  error occured.
 * When  such   event  occurs  on  a  context,  this  context  will  no  longer
 * generate new events and, thus, should be closed.
 * CONNLINE_EVENT_NO_BACKEND: when there is no backend available.
 * Either because it disappeared or just because there are none.  At that point
 * we cannot tell the status so it's up to the application to do something.
 * (acting as there would be a connection is the way to go)
 * CONNLINE_EVENT_DISCONNECTED: when the context is disconnected.
 * CONNLINE_EVENT_CONNECTED: when the context is connected.
 * Note: depending on backend, such connected event might occure twice:
 * - when it got a connection but is not yet online
 * - when its connection went online
 * Such behavior is currently proper to ConnMan backend. All other are directly
 * put online.
 * CONNLINE_EVENT_PROPERTY: when a property has its value changed.
 * @see connline_is_online()
 */
enum connline_event {
	CONNLINE_EVENT_ERROR          = 0,
	CONNLINE_EVENT_NO_BACKEND     = 1,
	CONNLINE_EVENT_DISCONNECTED   = 2,
	CONNLINE_EVENT_CONNECTED      = 3,
	CONNLINE_EVENT_PROPERTY       = 4,
};

/**
 * An opaque type representing a unique connline context.
 */
struct connline_context;

/**
 * Connline callback type definition
 * In order to  handle  connline  events,  callbacks  can  be  set  and  should
 * follow this signature.  Note that, since a  callback  gets  the  event which
 * triggers  it,  one   callback  can  handle  all  events  (all  the  ones  it
 * has been set for)
 * @param context a valid connline context on which the event occured
 * @param event the event responsible for triggering the callback
 * @param properties a table of key/value pair ended by  NULL.   This parameter
 * is relevant only for CONNLINE_EVENT_PROPERTY event.  The table  is made  of:
 * key - value - key - value - ... key - value ... - NULL
 * Keys and  values  will be  freed by  connline so  application needs to  copy
 * what's relevant to itself.
 * Keys are:
 * - bearer: the bearer name which put the context connected
 * - interface: the interface name of the network connection (eth0, wlan0...)
 * - address: the list of IPv4  and/or IPv6 address, separated by ','
 * @param user_data a pointer set previously or NULL instead
 * @see connline_is_online(), connline_get_bearer()
 */
typedef void (*connline_callback_f)(struct connline_context *context,
					enum connline_event event,
					const char **properties,
					void *user_data);

/**
 * Initialize Connline library according to the right event loop
 * @param event_loop_type a supported event loop type
 * @param data a pointer on a specific data depending on event loop type
 * This affects only CONNLINE_EVENT_LOOP_LIBEVENT,  data should be a pointer on
 * a valid struct event_base.
 * @return 0 on success or a negative value instead
 * @see connline_event_loop
 */
int connline_init(enum connline_event_loop event_loop_type, void *data);

/**
 * Request the context to open a connection
 * Depending on  the  connection  manager  daemon, this  might  lead  to  valid
 * network connection.
 * A background connection is when you are just requesting to be  notified when
 * a connection  has  been  set,  but you are  not  requesting  the  underlying
 * backend to get connected.
 * If a network connection happens and follows  the  bearer  type  you  set for
 * this context,  connected  event  will  be triggered  (if  only an  connected
 * callback was set)
 * @param context a valid connline context
 * @param background_connection true or false depending on  wether  you  want a
 * background connection or not.
 * @return 0 on success or a negative value instead
 */
struct connline_context *connline_open(enum connline_bearer bearer_type,
						bool background_connection,
						connline_callback_f callback,
						void *user_data);

/**
 * Return context's status
 * @param context a valid connline context
 * @return a boolean indicating context is online or not
 * Note:  on ConnMan backend, getting a CONNLINE_EVENT_CONNECTED  does not mean
 * the context went online. Check this status via this function.
 */
bool connline_is_online(struct connline_context *context);

/**
 * Return the context bearer in use when connected
 * @param context a valid connline context
 * @return an integer  representing  the bearer in  use. Note  if not connected
 * it  will return  CONNLINE_BEARER_UNKNOWN.
 * Note:  a human  readable value of the bearer  is provided  through the event
 * CONNLINE_EVENT_PROPERTY.
 * @see enum connline_bearer
 */
enum connline_bearer connline_get_bearer(struct connline_context *context);

/**
 * Close the context
 * Memory will be deallocated internally.
 * @param context a valid connline context
 * @see connline_open()
 */
void connline_close(struct connline_context *context);

/**
 * Final library cleanup
 * @see connline_init()
 */
void connline_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif

