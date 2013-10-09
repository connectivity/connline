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

#ifndef __CONNLINE_DBUS_H__
#define __CONNLINE_DBUS_H__

#include <dbus/dbus.h>
#include <stdbool.h>

#ifndef DBUS_TIMEOUT_USE_DEFAULT
	#define DBUS_TIMEOUT_USE_DEFAULT (-1)
#endif

#define DBUS_SERVICE_OWNER_CHANGED "NameOwnerChanged"

/* 
 * Used for connline_dbus_append_dict_entry_dict,
 * and connline_dbus_get_dict_entry. 
 * It should not be used anywhere else.
 */
enum connline_dbus_entry {
	CONNLINE_DBUS_ENTRY_BASIC       = 0,
	CONNLINE_DBUS_ENTRY_ARRAY       = 1,
	CONNLINE_DBUS_ENTRY_FIXED_ARRAY = 2,
	CONNLINE_DBUS_ENTRY_DICT        = 3,
};

typedef void (*connline_dbus_property_f) (DBusMessageIter *iter,
							void *user_data);
typedef bool (*connline_dbus_foreach_callback_f) (DBusMessageIter *iter,
							void *user_data);

static inline
bool connline_dbus_open_dict(DBusMessageIter *iter, DBusMessageIter *dict) {
	return dbus_message_iter_open_container(iter, DBUS_TYPE_ARRAY,
			DBUS_DICT_ENTRY_BEGIN_CHAR_AS_STRING
			DBUS_TYPE_STRING_AS_STRING DBUS_TYPE_VARIANT_AS_STRING
			DBUS_DICT_ENTRY_END_CHAR_AS_STRING, dict);
}

void connline_dbus_append_dict(DBusMessageIter *iter,
				const char *key_name,
				connline_dbus_property_f property_function,
				void *user_data);

void connline_dbus_append_basic(DBusMessageIter *iter,
				const char *key_name,
				int dbus_type,
				void *value);

void connline_dbus_append_array(DBusMessageIter *iter,
				const char *key_name,
				int dbus_type,
				connline_dbus_property_f property_function,
				void *user_data);

void connline_dbus_append_fixed_array(DBusMessageIter *iter,
					const char *key_name,
					int dbus_type,
					int length,
					void *value);

/* Use preferably the inline functions below this one */
void connline_dbus_append_dict_entry(DBusMessageIter *dict,
				const char *key_name,
				enum connline_dbus_entry entry_type,
				int dbus_type,
				int length,
				connline_dbus_property_f property_function,
				void *user_data);

static inline
void connline_dbus_append_dict_entry_basic(DBusMessageIter *dict,
							const char *key_name,
							int dbus_type,
							void *value)
{
	connline_dbus_append_dict_entry(dict, key_name,
			CONNLINE_DBUS_ENTRY_BASIC, dbus_type, 0, NULL, value);
}

static inline
void connline_dbus_append_dict_entry_array(DBusMessageIter *dict,
				const char *key_name,
				int dbus_type,
				connline_dbus_property_f property_function,
				void *user_data)
{
	connline_dbus_append_dict_entry(dict, key_name,
				CONNLINE_DBUS_ENTRY_ARRAY, dbus_type, 0,
				property_function, user_data);
}

static inline
void connline_dbus_append_dict_entry_fixed_array(DBusMessageIter *dict,
							const char *key_name,
							int dbus_type,
							int length,
							void *value)
{
	connline_dbus_append_dict_entry(dict, key_name,
				CONNLINE_DBUS_ENTRY_FIXED_ARRAY, dbus_type,
				length, NULL, value);
}

static inline
void connline_dbus_append_dict_entry_dict(DBusMessageIter *dict,
				const char *key_name,
				connline_dbus_property_f property_function,
				void *user_data)
{
	connline_dbus_append_dict_entry(dict, key_name,
				CONNLINE_DBUS_ENTRY_DICT, 0, 0,
				property_function, user_data);
}

int connline_dbus_get_basic(DBusMessageIter *iter,
					int dbus_type,
					void *destination);

int connline_dbus_get_basic_variant(DBusMessageIter *iter,
						int dbus_type,
						void *destination);

int connline_dbus_get_array(DBusMessageIter *iter,
					int dbus_type,
					int *length,
					void *destination);

int connline_dbus_get_fixed_array(DBusMessageIter *iter,
					int *length,
					void *destination);


int connline_dbus_foreach_dict_entry(DBusMessageIter *iter,
				connline_dbus_foreach_callback_f callback,
				void *user_data);


/* Use preferably the inline functions below this one */
int connline_dbus_get_dict_entry(DBusMessageIter *iter,
					const char *key_name,
					enum connline_dbus_entry entry_type,
					int dbus_type,
					int *length,
					void *destination);

static inline
int connline_dbus_get_dict_entry_basic(DBusMessageIter *iter,
						const char *key_name,
						int dbus_type,
						void *destination)
{
	return connline_dbus_get_dict_entry(iter, key_name,
					CONNLINE_DBUS_ENTRY_BASIC,
					dbus_type, NULL, destination);
}

static inline
int connline_dbus_get_dict_entry_array(DBusMessageIter *iter,
						const char *key_name,
						int dbus_type,
						int *length,
						void *destination)
{
	return connline_dbus_get_dict_entry(iter, key_name,
					CONNLINE_DBUS_ENTRY_ARRAY,
					dbus_type, length, destination);
}

static inline
int connline_dbus_get_dict_entry_fixed_array(DBusMessageIter *iter,
							const char *key_name,
							int dbus_type,
							int *length,
							void *destination)
{
	return connline_dbus_get_dict_entry(iter, key_name,
					CONNLINE_DBUS_ENTRY_FIXED_ARRAY,
					dbus_type, length, destination);
}

static inline
int connline_dbus_get_dict_entry_dict(DBusMessageIter *iter,
					const char *key_name,
					DBusMessageIter *dict)
{
	return connline_dbus_get_dict_entry(iter, key_name,
					CONNLINE_DBUS_ENTRY_DICT,
					DBUS_TYPE_INVALID, NULL, dict);
}

/* Use preferably the inline functions below this one */
int connline_dbus_get_struct_entry(DBusMessageIter *iter,
					unsigned int position,
					enum connline_dbus_entry entry_type,
					int dbus_type,
					int *length,
					void *destination);

static inline
int connline_dbus_get_struct_entry_basic(DBusMessageIter *iter,
						unsigned int position,
						int dbus_type,
						void *destination)
{
	return connline_dbus_get_struct_entry(iter, position,
					CONNLINE_DBUS_ENTRY_BASIC,
					dbus_type, NULL, destination);
}

static inline
int connline_dbus_get_struct_entry_array(DBusMessageIter *iter,
						unsigned int position,
						int dbus_type,
						int *length,
						void *destination)
{
	return connline_dbus_get_struct_entry(iter, position,
					CONNLINE_DBUS_ENTRY_ARRAY,
					dbus_type, length, destination);
}

static inline
int connline_dbus_get_struct_entry_fixed_array(DBusMessageIter *iter,
						unsigned int position,
						int dbus_type,
						int *length,
						void *destination)
{
	return connline_dbus_get_struct_entry(iter, position,
					CONNLINE_DBUS_ENTRY_FIXED_ARRAY,
					dbus_type, length, destination);
}

static inline
int connline_dbus_get_struct_entry_dict(DBusMessageIter *iter,
						unsigned int position,
						DBusMessageIter *dict)
{
	return connline_dbus_get_struct_entry(iter, position,
					CONNLINE_DBUS_ENTRY_DICT,
					DBUS_TYPE_INVALID, NULL, dict);
}

static inline dbus_bool_t connline_dbus_is_service_running(DBusConnection *dbus_cnx,
					const char *dbus_service_name)
{
	if (dbus_bus_name_has_owner(dbus_cnx,
					dbus_service_name, NULL) == FALSE)
		return FALSE;

	return TRUE;
}

int connline_dbus_setup_watch(DBusConnection *dbus_cnx,
					const char *rule,
					DBusHandleMessageFunction filter,
					void *user_data);

void connline_dbus_remove_watch(DBusConnection *dbus_cnx,
					const char *rule,
					DBusHandleMessageFunction filter,
					void *user_data);

#endif /* __CONNLINE_DBUS_H__ */
