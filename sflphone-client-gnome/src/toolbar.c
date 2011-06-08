/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
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

#include <toolbar.h>
#include <contacts/addressbook.h>


static void
call_mailbox (GtkWidget* widget UNUSED, gpointer data UNUSED)
{
    account_t* current;
    callable_obj_t *mailbox_call;
    gchar *to, *from, *account_id;

    current = account_list_get_current ();

    if (current == NULL)  // Should not happens
        return;

    to = g_strdup (g_hash_table_lookup (current->properties, ACCOUNT_MAILBOX));
    account_id = g_strdup (current->accountID);

    create_new_call (CALL, CALL_STATE_DIALING, "", account_id, _ ("Voicemail"), to, &mailbox_call);
    DEBUG ("Call: TO : %s" , mailbox_call->_peer_number);
    calllist_add (current_calls , mailbox_call);
    calltree_add_call (current_calls, mailbox_call, NULL);
    update_actions();
    sflphone_place_call (mailbox_call);
    calltree_display (current_calls);
}

/**
 * Make a call
 */
static void
call_button (GtkWidget *widget UNUSED, gpointer   data UNUSED)
{
    callable_obj_t * selectedCall;
    callable_obj_t* new_call;

    selectedCall = calltab_get_selected_call (active_calltree);

    if (calllist_get_size (current_calls) >0)
        sflphone_pick_up();

    else if (calllist_get_size (active_calltree) > 0) {
        if (selectedCall) {
            create_new_call (CALL, CALL_STATE_DIALING, "", "", "", selectedCall->_peer_number, &new_call);

            calllist_add (current_calls, new_call);
            calltree_add_call (current_calls, new_call, NULL);
            sflphone_place_call (new_call);
            calltree_display (current_calls);
        } else {
            sflphone_new_call();
            calltree_display (current_calls);
        }
    } else {
        sflphone_new_call();
        calltree_display (current_calls);
    }
}
