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


typedef struct
{
    gchar * alias;  // persistent
    gchar * uri;    // persistent
    gchar * acc;    // persistent
    gboolean subscribed; // is subscription active
    gboolean status;    // Online/Offline
    gchar * note;   // more detailed status
}buddy_t;

/**
 * This function inits the buddy list from the client's gsettings schema.
 * @param client The given client which provide the schema
 */
void presence_list_init(SFLPhoneClient *client);

/**
 * This function clears the buddy list.
 */
void presence_list_flush();

/**
 * This function saved a modified buddy and resubscribes if necessary.
 * @param buddy A known buddy but with new information to be saved.
 * @param backup A backup of this buddy before it was changed
 */
void presence_list_update_buddy(buddy_t * buddy, buddy_t *backup);

/**
 * This function adds a buddy in the list.
 * @param buddy The buddy structure to be added.
 */
void presence_list_add_buddy(buddy_t * buddy);

/**
 * This function removes a buddy from the list.
 * @param buddy The buddy structure to be removed.
 */
void presence_list_remove_buddy(buddy_t * buddy);

/**
 * This function returns the number of buddies in list.
 * @return guint The size of the buddy list.
 */
guint presence_list_get_size();

/**
 * This function returns a pointer to the buddy list.
 * @return Glist * The pointer to the list.
 */
GList * presence_list_get();

/**
 * This function returns the nth buddy of the list.
 * @return buddy_t * The pointer to the nth buddy.
 */
buddy_t * presence_list_get_nth(guint n);

/**
 * This function returns a buddy which matches with params.
 * @param accID The account ID associated to the buddy.
 * @param uri The buddy's uri.
 * @return Glist * The pointer to the found buddy.
 */
buddy_t * presence_list_buddy_get_by_string(const gchar *accID, const gchar *uri);

/**
 * This function detects if the buddy already exists, based
 * on its accountID and URI and return the pointer to the real element
 * of the list.
 * @param buddy The buddy to be found in the list.
 * @return buddy_t *  The pointer to the buddy if it exist and NULL if not.
 */
buddy_t * presence_list_get_buddy(buddy_t * buddy);

/**
 * This function calls the dbus method to subscribe to all the buddies.
 * of a given account.
 * @param accID The account ID of the given account
 * @param flag True to subscribe and False to unsubscribe
 */
void presence_list_send_subscribes(account_t *acc, gboolean flag);

/**
 * This function create a new buddy with default value.
 * @return buddy_t The pointer to the new buddy.
 */
buddy_t * presence_create_buddy();

/**
 * This function print the entire list for debugging purpose.
 */
void presence_list_print();

static const char *const PRESENCE_STATUS_ONLINE = "Online";
static const char *const PRESENCE_STATUS_OFFLINE = "Offline";

#endif
