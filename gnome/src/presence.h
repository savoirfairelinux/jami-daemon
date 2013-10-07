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
#include "accountlist.h"


typedef struct prout
{
    gchar * alias;  // persistent
    gchar * uri;    // persistent
    gchar * acc;    // persistent
    gboolean subscribed; // is subscription active
    gboolean status;    // Online/Offline
    gchar * note;   // more detailed status
}buddy_t;

void presence_list_init(SFLPhoneClient *client);
void presence_list_load(SFLPhoneClient *client);
void presence_list_save(SFLPhoneClient *client);
void presence_list_flush();
void presence_list_add_buddy(buddy_t * buddy);
void presence_list_remove_buddy(buddy_t * buddy);
guint presence_list_get_size();
GList * presence_list_get();
buddy_t * presence_list_get_nth(guint n);
buddy_t * presence_list_buddy_get_by_string(const gchar *accID, const gchar *uri);
GList * presence_list_get_buddy(buddy_t * buddy);
void presence_list_send_subscribes(account_t *acc, gboolean flag);

static const char *const PRESENCE_STATUS_ONLINE = "Online";
static const char *const PRESENCE_STATUS_OFFLINE = "Offline";


#endif
