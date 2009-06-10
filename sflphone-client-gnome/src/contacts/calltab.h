/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Antoine Reversat <antoine.reversat@savoirfairelinux.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __CALLTAB_H__
#define __CALLTAB_H__

#include <calllist.h>
#include <gtk/gtk.h>

calltab_t* active_calltree;
calltab_t* current_calls;
calltab_t* history;
calltab_t* contacts;

calltab_t* calltab_init (gboolean searchbar_type, gchar *name);

/** Mark a call as selected.  There can be only one selected call.  This call
  * is the currently highlighted one in the list.
  * @param c The call */
void
calltab_select_call (calltab_t*, callable_obj_t *);

/** Return the selected call.
  * @return The number of the caller */
callable_obj_t *
calltab_get_selected_call (calltab_t*);

void
calltab_create_searchbar (calltab_t *);

#endif
