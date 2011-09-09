/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include "calllist.h"
#include "calltree.h"
#include "contacts/searchbar.h"
#include "eel-gconf-extensions.h"

static
gint is_callID_callstruct(gconstpointer a, gconstpointer b)
{
    const QueueElement *c = a;
    if(c == NULL || c->type != HIST_CALL)
	return 1;

    return g_strcasecmp(c->elem.call->_callID, (const gchar *) b);
}

// TODO : sflphoneGTK : try to do this more generic
void calllist_add_contact (gchar *contact_name, gchar *contact_phone, contact_type_t type, GdkPixbuf *photo)
{
    /* Check if the information is valid */
    if (g_strcasecmp (contact_phone, EMPTY_ENTRY) == 0)
        return;

    callable_obj_t *new_call = create_new_call (CONTACT, CALL_STATE_DIALING, "", "", contact_name, contact_phone);

    // Attach a pixbuf to a contact
    if (photo) {
        new_call->_contact_thumbnail = gdk_pixbuf_copy (photo);
    } else {
        GdkPixbuf *pixbuf;
        switch (type) {
            case CONTACT_PHONE_BUSINESS:
                pixbuf = gdk_pixbuf_new_from_file (ICONS_DIR "/users.svg", NULL);
                break;
            case CONTACT_PHONE_HOME:
                pixbuf = gdk_pixbuf_new_from_file (ICONS_DIR "/home.svg", NULL);
                break;
            case CONTACT_PHONE_MOBILE:
                pixbuf = gdk_pixbuf_new_from_file (ICONS_DIR "/phone.svg", NULL);
                break;
            default:
                pixbuf = gdk_pixbuf_new_from_file (ICONS_DIR "/contact_default.svg", NULL);
                break;
        }

        new_call->_contact_thumbnail = pixbuf;
    }

    calllist_add_call (contacts, new_call);
    calltree_add_call (contacts, new_call, NULL);
}

/*
 * Function passed to calllist_clean to free every QueueElement.
 */
static void
calllist_free_element(gpointer data, gpointer user_data UNUSED)
{
    QueueElement *element = data;
    if (element->type == HIST_CONFERENCE)
        free_conference_obj_t (element->elem.conf);
    else /* HIST_CALL */
        free_callable_obj_t (element->elem.call);
    g_free (element);
}

void
calllist_clean (calltab_t* tab)
{
    g_queue_foreach (tab->callQueue, calllist_free_element, NULL);
    g_queue_free (tab->callQueue);
}

void
calllist_reset (calltab_t* tab)
{
    calllist_clean (tab);
    tab->callQueue = g_queue_new();
}

void calllist_add_history_call (callable_obj_t *obj)
{
    if (eel_gconf_get_integer (HISTORY_ENABLED)) {
        QueueElement *element = g_new0(QueueElement, 1);
        element->type = HIST_CALL;
        element->elem.call = obj;
        g_queue_push_tail (history->callQueue, (gpointer) element);
        calltree_add_call (history, obj, NULL);
    }
}

void calllist_add_history_conference(conference_obj_t *obj)
{
    if(eel_gconf_get_integer (HISTORY_ENABLED)) {
        QueueElement *element = g_new0(QueueElement, 1);
        element->type = HIST_CONFERENCE;
        element->elem.conf = obj;
        g_queue_push_tail (history->callQueue, (gpointer) element);
        calltree_add_conference (history, obj);
    }
}

void
calllist_add_call (calltab_t* tab, callable_obj_t * c)
{
    DEBUG("Calllist: Add Call %s", c->_callID);

    QueueElement *element = g_new0(QueueElement, 1);
    element->type = HIST_CALL;
    element->elem.call = c;
    g_queue_push_tail(tab->callQueue, (gpointer) element);
}

void
calllist_clean_history (void)
{
    guint size = calllist_get_size (history);
    for (guint i = 0; i < size; i++) {
        QueueElement* c = calllist_get_nth(history, i);
        if (c->type == HIST_CALL)
            calltree_remove_call (history, c->elem.call, NULL);
        else if(c->type == HIST_CONFERENCE)
            calltree_remove_conference (history, c->elem.conf, NULL);
    }

    calllist_reset (history);
}

void
calllist_remove_from_history (callable_obj_t* c)
{
    calllist_remove_call(history, c->_callID);
    calltree_remove_call(history, c, NULL);
}

void
calllist_remove_call (calltab_t* tab, const gchar * callID)
{
    DEBUG("CallList: Remove call %s from list", callID);

    GList *c = g_queue_find_custom (tab->callQueue, callID, is_callID_callstruct);
    if (c == NULL) {
        DEBUG("CallList: Could not remove call %s", callID);
    	return;
    }

    QueueElement *element = (QueueElement *)c->data;
    if (element->type != HIST_CALL) {
        ERROR("CallList: Error: Element %s is not a call", callID);
        return;
    }

    g_queue_remove(tab->callQueue, element);

    calllist_add_call(history, element->elem.call);
    calltree_add_call(history, element->elem.call, NULL);
}


callable_obj_t *
calllist_get_by_state(calltab_t* tab, call_state_t state)
{
    GList * c = g_queue_find_custom(tab->callQueue, &state, get_state_callstruct);
    return c ? c->data : NULL;
}

guint
calllist_get_size (calltab_t* tab)
{
    return g_queue_get_length (tab->callQueue);
}

QueueElement *
calllist_get_nth (calltab_t* tab, guint n)
{
    return g_queue_peek_nth (tab->callQueue, n);
}

callable_obj_t *
calllist_get_call (calltab_t* tab, const gchar * callID)
{
    DEBUG("CallList: Get call: %s", callID);

    GList * c = g_queue_find_custom (tab->callQueue, callID, is_callID_callstruct);
    if (c == NULL) {
        ERROR("CallList: Error: Could not find call %s", callID);
        return NULL;
    }

    QueueElement *element = c->data;
    if (element->type != HIST_CALL) {
        ERROR("CallList: Error: Element %s is not a call", callID);
        return NULL;
    }

    return element->elem.call;
}
