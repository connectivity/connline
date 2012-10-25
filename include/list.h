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

#ifndef __LIST_H__
#define __LIST_H__

#include <stdlib.h>

struct _dlist;
typedef struct _dlist dlist;

typedef void (*dlist_data_cb_f)(void *data);

static inline void dlist_free(dlist *list)
{
	free(list);
}

void dlist_free_all(dlist *list);

dlist *dlist_prepend(dlist *list, void *data);

dlist *dlist_remove(dlist *list, void *data);

void dlist_foreach(dlist *list, dlist_data_cb_f callback);

#endif /* __LIST_H__ */
