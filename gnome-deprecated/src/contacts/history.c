/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include <string.h>
#include "history.h"
#include "calltree.h"
#include "searchbar.h"
#include "calltab.h"

static GtkTreeModel *history_filter;
static GtkEntry *history_searchbar_widget;

static gboolean
search_type_matches_state(SearchType type, const gchar *state)
{
    switch (type) {
        case SEARCH_MISSED:
            return !g_strcmp0(state, MISSED_STRING);
        case SEARCH_INCOMING:
            return !g_strcmp0(state, INCOMING_STRING);
        case SEARCH_OUTGOING:
            return !g_strcmp0(state, OUTGOING_STRING);
        default:
            return FALSE;
    }
}

static gboolean
history_is_visible(GtkTreeModel* model, GtkTreeIter* iter, G_GNUC_UNUSED gpointer data)
{
    gboolean visible = TRUE;
    // Fetch the call description
    const gchar *text = NULL;
    const gchar *id = NULL;
    gtk_tree_model_get(model, iter, COLUMN_ACCOUNT_DESC, &text, COLUMN_ID, &id, -1);
    if (!id)
        return visible;
    callable_obj_t *history_entry = calllist_get_call(history_tab, id);

    if (text && history_entry) {
        // Filter according to the type of call
        // MISSED, INCOMING, OUTGOING, ALL
        const gchar* search = gtk_entry_get_text(history_searchbar_widget);

        if (!search || !*search)
            return TRUE;

        SearchType search_type = get_current_history_search_type();
        visible = g_regex_match_simple(search, text, G_REGEX_CASELESS, 0);

        if (search_type == SEARCH_ALL)
            return visible;
        else // We need a match on the history_state and the current search type
            visible = visible && search_type_matches_state(search_type, history_entry->_history_state);
    }

    return visible;
}

static GtkTreeModel* history_create_filter(GtkTreeModel* child)
{
    GtkTreeModel* ret = gtk_tree_model_filter_new(child, NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(ret), history_is_visible, NULL, NULL);
    return GTK_TREE_MODEL(ret);
}

void history_search()
{
    if (history_filter != NULL)
        gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(history_filter));
}

void history_search_init(void)
{
    history_filter = history_create_filter(GTK_TREE_MODEL(history_tab->store));
    gtk_tree_view_set_model(GTK_TREE_VIEW(history_tab->view), GTK_TREE_MODEL(history_filter));
}

void history_set_searchbar_widget(GtkWidget *searchbar)
{
    history_searchbar_widget = GTK_ENTRY(searchbar);
}
