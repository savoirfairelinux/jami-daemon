/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef __CALLTAB_H__
#define __CALLTAB_H__

#include "calllist.h"
#include "conferencelist.h"
#include "sflphone_client.h"
#include <gtk/gtk.h>

calltab_t* active_calltree_tab;
calltab_t* current_calls_tab;
calltab_t* history_tab;
calltab_t* contacts_tab;

calltab_t* calltab_init(gboolean, const gchar * const, SFLPhoneClient *client);

/** Mark a call as selected.  There can be only one selected call.  This call
  * is the currently highlighted one in the list.
  * @param c The call */
void
calltab_select_call(calltab_t *, callable_obj_t *);

void
calltab_select_conf(calltab_t *, conference_obj_t *);

gint
calltab_get_selected_type(calltab_t *);

/** Return the selected call.
  * @return The number of the caller */
callable_obj_t *
calltab_get_selected_call(calltab_t *);

conference_obj_t *
calltab_get_selected_conf(calltab_t *);

void
calltab_create_searchbar(calltab_t *, SFLPhoneClient *client);

gboolean
calltab_has_name(calltab_t *tab, const gchar *name);

#endif
