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

#include <connline/list.h>

struct _dlist {
	struct _dlist *next;
	struct _dlist *prev;

	void          *data;
};

static inline dlist *dlist_new(void)
{
	return calloc(1, sizeof(dlist));
}

void dlist_free_all(dlist *list)
{
	for(; list != NULL; list = list->next)
		dlist_free(list);
}

dlist *dlist_prepend(dlist *list, void *data)
{
	dlist *p_list;

	p_list = dlist_new();
	if (p_list == NULL)
		return list;

	p_list->data = data;

	if (list != NULL) {
		list->prev = p_list;
		p_list->next = list;
	}

	return p_list;
}

static dlist *dlist_remove_link(dlist *list, dlist *link)
{
	if (link == NULL || list == NULL)
		return list;

	if (list == link) {
		list = link->next;
		if (list != NULL)
			list->prev = NULL;
	} else {
		dlist *prev = link->prev;
		dlist *next = link->next;

		prev->next = next;
		if (next != NULL)
			next->prev = prev;
	}

	link->next = NULL;
	link->prev = NULL;

	return list;
}

dlist *dlist_remove(dlist *list, void *data)
{
	dlist *cur;

	for (cur = list; cur != NULL && cur->data != data; cur = cur->next);

	if (cur == NULL)
		return list;

	list = dlist_remove_link(list, cur);

	dlist_free(cur);

	return list;
}

void dlist_foreach(dlist *list, dlist_data_cb_f callback)
{
	if (callback == NULL)
		return;

	for (; list != NULL; list = list->next)
		callback(list->data);
}
