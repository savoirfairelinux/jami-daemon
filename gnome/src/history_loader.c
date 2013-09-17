/*
 *  Copyright (C) 2012-2013 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
 *  Derived from this example by Emmanuelle Bassi :
 *      http://blogs.gnome.org/ebassi/documentation/lazy-loading/
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

#include "history_loader.h"
#include <gtk/gtk.h>
#include "callable_obj.h"
#include "contacts/history.h"
#include "dbus.h"
#include "calllist.h"
#include "calltree.h"

enum {
    STATE_STARTED,  /* start state */
    STATE_LOADING,  /* feeding items to the store */
    STATE_COMPLETE, /* feeding the store to the view */
    STATE_FINISHED  /* finish state - not used */
};

static void
create_callable_from_entry(calltab_t *tab, GHashTable *entry)
{
    callable_obj_t *history_call = create_history_entry_from_hashtable(entry);

    /* Add it and update the GUI */
    calllist_add_call_to_front(tab, history_call);
    calltree_add_history_entry(history_call);
}

static gboolean
load_items(gpointer data)
{
    IdleData *id = data;

    /* make sure we're in the right state */
    g_assert(id->load_state == STATE_STARTED || id->load_state == STATE_LOADING);

    /* empty list */
    if (!id->items) {
        /* we either have to call the asynchronous dbus_get_history, or if it's
         * been called but has not finished yet, try again later */
        if (!id->dbus_called) {
            dbus_get_history(id);
            id->dbus_called = TRUE;
            return TRUE;
        } else if (!id->dbus_finished)
            return TRUE;
    }

    /* We made the call but the list was empty, so we're done */
    if (!id->items || id->items->len == 0) {
        id->load_state = STATE_COMPLETE;
        return FALSE;
    }

    /* is this the first run */
    if (!id->n_items) {
        id->n_items = id->items->len;
        id->n_loaded = 0;
        id->load_state = STATE_LOADING;
    }

    /* add items back to front in the list */
    const gint idx = id->items->len - 1 - id->n_loaded;
    g_assert(idx >= 0);
    GHashTable *entry = g_ptr_array_index(id->items, idx);
    g_assert(entry != NULL);
    create_callable_from_entry(id->tab, entry);

    ++id->n_loaded;
    if (id->n_loaded == id->n_items) {
        /* we loaded everything, so we can change state
         * and remove the idle callback function; after
         * using the cleanup_load_items function will be
         * called
         */
        id->load_state = STATE_COMPLETE;
        id->n_loaded = 0;
        id->n_items = 0;
        id->items = NULL;
        return FALSE;
    } else
        return TRUE;
}

static void
cleanup_load_items(gpointer data)
{
    IdleData *id = data;
    g_assert(id->load_state == STATE_COMPLETE);
    /* this will actually load the model inside the view */
    history_search_init();
    gtk_widget_show(id->tab->tree);
    g_free(id);
}

void
lazy_load_items(calltab_t *tab)
{
    IdleData *data = g_new(IdleData, 1);
    data->load_state = STATE_STARTED;
    data->tree_store = tab->store;
    data->tree_view = tab->view;
    data->n_items = 0;
    data->n_loaded = 0;
    data->items = NULL;
    data->tab = tab;
    data->dbus_called = FALSE;
    data->dbus_finished = FALSE;
    data->load_id = g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                                    load_items,
                                    data,
                                    cleanup_load_items);
}
