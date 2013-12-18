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

#ifndef __CONNLINE_UTILS_H__
#define __CONNLINE_UTILS_H__

#include <config.h>
#include <connline/connline.h>

#ifdef DEBUG
#include <stdio.h>
#define DBG(fmt, arg...) { \
	fprintf(stdout, "DBG:%s:%s() " fmt "\n", \
		__FILE__, __FUNCTION__ , ## arg); \
}
#else
#define DBG(fmt, arg...) {}
#endif

char *get_process_name(void);

char *get_new_unique_name(void);

char **insert_into_property_list(char **properties,
					const char *name,
					const char *value);

void property_list_free(char **properties);

const char *connline_bearer_to_string(enum connline_bearer bearer);

#endif
