/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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

#include "calllist.h"
#include "str_utils.h"
#include <string.h>
#include "calltab.h"
#include "calltree.h"

// Must return 0 when a match is found
static gint
is_callID_callstruct(gconstpointer a, gconstpointer b)
{
    const callable_obj_t *c = a;

    // if it's null or not a call it's not the call we're looking for
    if (c == NULL) {
        g_warning("NULL element in list");
        return 1;
    }

    return g_strcmp0(c->_callID, (const gchar *) b);
}

// TODO : try to do this more generically
void calllist_add_contact(gchar *contact_name, gchar *contact_phone, contact_type_t type, GdkPixbuf *photo)
{
    /* Check if the information is valid */
    if (!contact_phone)
        return;

    callable_obj_t *new_call = create_new_call(CONTACT, CALL_STATE_DIALING, "", "", contact_name, contact_phone);

    // Attach a pixbuf to a contact
    if (photo)
        new_call->_contact_thumbnail = gdk_pixbuf_copy(photo);
    else {
        GdkPixbuf *pixbuf;

        switch (type) {
            case CONTACT_PHONE_BUSINESS:
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/users.svg", NULL);
                break;
            case CONTACT_PHONE_HOME:
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/home.svg", NULL);
                break;
            case CONTACT_PHONE_MOBILE:
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/phone.svg", NULL);
                break;
            default:
                pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/contact_default.svg", NULL);
                break;
        }

        new_call->_contact_thumbnail = pixbuf;
    }

    calllist_add_call(contacts_tab, new_call);
    calltree_add_call(contacts_tab, new_call, NULL);
}

/*
 * Function passed to calllist_clean to free every callable_obj_t.
 */
static void
calllist_free_element(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
    callable_obj_t *call = data;
    free_callable_obj_t(call);
}

gboolean
calllist_empty(calltab_t *tab)
{
    g_return_val_if_fail(tab != NULL && tab->callQueue != NULL, FALSE);
    return !calllist_get_size(tab);
}

void
calllist_clean(calltab_t* tab)
{
    g_return_if_fail(tab != NULL && tab->callQueue != NULL);
    g_queue_foreach(tab->callQueue, calllist_free_element, NULL);
    g_queue_free(tab->callQueue);
    tab->callQueue = 0;
}

void
calllist_reset(calltab_t* tab)
{
    g_return_if_fail(tab != NULL);
    calllist_clean(tab);
    tab->callQueue = g_queue_new();
}

void
calllist_add_call(calltab_t* tab, callable_obj_t * c)
{
    g_debug("Adding call with callID %s to tab %s", c->_callID, tab->name);
    g_queue_push_tail(tab->callQueue, c);
    g_debug("Tab %s has %d calls", tab->name, calllist_get_size(tab));
}

void
calllist_add_call_to_front(calltab_t* tab, callable_obj_t * c)
{
    g_return_if_fail(tab != NULL && tab->callQueue != NULL);
    g_queue_push_head(tab->callQueue, c);
}

void
calllist_clean_history()
{
    guint size = calllist_get_size(history_tab);

    for (guint i = 0; i < size; i++) {
        callable_obj_t * c = calllist_get_nth(history_tab, i);
        if (c)
            calltree_remove_call(history_tab, c->_callID);
    }

    calllist_reset(history_tab);
}

void
calllist_remove_from_history(callable_obj_t* c, SFLPhoneClient *client)
{
    calllist_remove_call(history_tab, c->_callID, client);
    calltree_remove_call(history_tab, c->_callID);
}

void
calllist_remove_call(calltab_t* tab, const gchar * callID, SFLPhoneClient *client)
{
    g_return_if_fail(tab != NULL && tab->callQueue != NULL);
    GList *c = g_queue_find_custom(tab->callQueue, callID, is_callID_callstruct);

    if (c == NULL)
        return;

    callable_obj_t *call = c->data;

    if (!g_queue_is_empty(tab->callQueue)) {
        g_debug("Removing call %s from tab %s", callID, tab->name);
        g_queue_remove(tab->callQueue, call);
    }

    /* If removing a call from the history, don't add it back to the history! */
    if (calltab_has_name(tab, HISTORY))
        return;

    /* Don't save empty (i.e. started dialing, then deleted) calls */
    if (call->_peer_number && strlen(call->_peer_number) > 0) {
        calllist_add_call(history_tab, call);
        if (g_settings_get_boolean(client->settings, "history-enabled"))
            calltree_add_history_entry(call);
    }
}

callable_obj_t *
calllist_get_by_state(calltab_t* tab, call_state_t state)
{
    GList * c = g_queue_find_custom(tab->callQueue, &state, get_state_callstruct);
    return c ? c->data : NULL;
}

guint
calllist_get_size(const calltab_t* tab)
{
    return g_queue_get_length(tab->callQueue);
}

callable_obj_t*
calllist_get_nth(calltab_t* tab, guint n)
{
    return g_queue_peek_nth(tab->callQueue, n);
}

callable_obj_t *
calllist_get_call(calltab_t* tab, const gchar * callID)
{
    g_return_val_if_fail(tab, NULL);

    GList * c = g_queue_find_custom(tab->callQueue, callID, is_callID_callstruct);

    if (c == NULL) {
        g_warning("Could not find call %s in tab %s", callID, tab->name);
        return NULL;
    }

    return c->data;
}
