/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include <history.h>
#include <string.h>
#include <searchbar.h>
#include <calltab.h>

static GtkTreeModel *history_filter;
static GtkEntry *history_searchbar_widget;

static gboolean history_is_visible(GtkTreeModel* model, GtkTreeIter* iter, gpointer data UNUSED)
{
    gboolean ret = TRUE;
    callable_obj_t *history_entry = NULL;
    const gchar *text = NULL;

    // Fetch the call description
    GValue val;
    memset(&val, 0, sizeof val);
    gtk_tree_model_get_value(GTK_TREE_MODEL(model), iter, 1, &val);

    if (G_VALUE_HOLDS_STRING(&val))
        text = (gchar *) g_value_get_string(&val);

    // Fetch the call type
    GValue obj;
    memset(&obj, 0, sizeof obj);
    gtk_tree_model_get_value(GTK_TREE_MODEL(model), iter, 3, &obj);

    if (G_VALUE_HOLDS_POINTER(&obj))
        history_entry = (gpointer) g_value_get_pointer(&obj);

    if (text && history_entry) {
        // Filter according to the type of call
        // MISSED, INCOMING, OUTGOING, ALL
        const gchar* search = gtk_entry_get_text(history_searchbar_widget);

        if (!search || !*search)
            goto end;

        SearchType search_type = get_current_history_search_type();
        ret = g_regex_match_simple(search, text, G_REGEX_CASELESS, 0);

        if (search_type == SEARCH_ALL)
            goto end;
        else // We need a match on the history_state_t and the current search type
            ret = ret && (history_entry->_history_state + 1) == search_type;
    }

end:
    g_value_unset(&val);
    return ret;
}

static GtkTreeModel* history_create_filter(GtkTreeModel* child)
{
    GtkTreeModel* ret = gtk_tree_model_filter_new(child, NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(ret), history_is_visible, NULL, NULL);
    return GTK_TREE_MODEL(ret);
}

void history_search(void)
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
