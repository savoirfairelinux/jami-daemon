/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Patrick Keroulas <patrick.keroulas@savoirfairelinux.com>
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

#ifndef __PRESENCE_H__
#define __PRESENCE_H__

#include <glib.h>

#include "sflphone_client.h"


typedef struct prout
{
    gchar * alias;  // persistent
    gchar * uri;    // persistent
    gchar * acc;    //persistent
    gboolean subscribed; // is subscription active
    gboolean status;    // Online/Offline
}buddy_t;

void presence_init(SFLPhoneClient *client);
void presence_load_list(SFLPhoneClient *client, GList *list);
void presence_save_list(SFLPhoneClient *client, GList * list);
void presence_flush_list(GList * list);
void presence_add_buddy(GList * list, buddy_t * buddy);
void presence_remove_buddy(GList * list, buddy_t * buddy);
guint presence_list_get_size(GList * list);
GList * presence_get_list();
buddy_t * presence_list_get_nth(GList * list, guint n);
GList * presence_get_buddy(GList * list, buddy_t * buddy);


#endif
