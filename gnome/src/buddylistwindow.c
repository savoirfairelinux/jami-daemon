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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>

#include "buddylistwindow.h"
#include "account_schema.h"
#include "accountlist.h"
#include "actions.h"
#include "presence.h"
#include "dbus.h"
#include "str_utils.h"

static GtkWidget *buddylistwindow;
static GtkWidget *vbox;

static GtkTreeModel *create_and_fill_model (void);
static GtkWidget * create_view (void);
gboolean selection_changed(GtkTreeSelection *selection);

enum
{
    COLUMN_BUDDIES,
    COLUMN_STATUS,
    COLUMN_NOTE,
    COLUMN_SUBSCRIBED
 // NUM_COLS = 2
};

#define N_COLUMN 4


static GtkTreeModel *
create_and_fill_model (void)
{
    GtkTreeStore *treestore;
    GtkTreeIter toplevel, child;
    treestore = gtk_tree_store_new(N_COLUMN, G_TYPE_STRING, // Alias
                                             G_TYPE_STRING, // Status
                                             G_TYPE_STRING, // Note
                                             G_TYPE_STRING);// (temp) subscribed

    GList * buddy_list = presence_get_list();
    buddy_t * buddy;

    for (guint i = 0; i < account_list_get_size(); i++)
    {
        account_t * acc = account_list_get_nth(i);
        if (acc->state == (ACCOUNT_STATE_REGISTERED))
        {
            gtk_tree_store_append(treestore, &toplevel, NULL);
            gtk_tree_store_set(treestore, &toplevel,
                    COLUMN_BUDDIES, (gchar*) account_lookup(acc, CONFIG_ACCOUNT_ALIAS),
                    COLUMN_STATUS, "",
                    COLUMN_NOTE, "",
                    COLUMN_SUBSCRIBED, "",
                    -1);
            for (guint j =  1; j < presence_list_get_size(buddy_list); j++)
            {
                buddy = presence_list_get_nth(buddy_list, j);
                if(g_strcmp0(buddy->acc, (gchar*)account_lookup(acc, CONFIG_ACCOUNT_ID))==0)
                {
                    gtk_tree_store_append(treestore, &child, &toplevel);
                    gtk_tree_store_set(treestore, &child,
                        COLUMN_BUDDIES, buddy->uri,
                        COLUMN_STATUS, (buddy->status)? "Online":"Offline",
                        COLUMN_NOTE,  buddy->note,
                        COLUMN_SUBSCRIBED, (buddy->subscribed)? "Active":"Inactive",
                        -1);
                }
            }
        }
    }
    return GTK_TREE_MODEL(treestore);
}


gboolean
selection_changed(GtkTreeSelection *selection) {
    GtkTreeView *treeView;
    GtkTreeModel *model;
    GtkTreeIter iter;
    gchar *active;

    treeView = gtk_tree_selection_get_tree_view(selection);
    model = gtk_tree_view_get_model(treeView);
    gtk_tree_selection_get_selected(selection, &model, &iter);
    gtk_tree_model_get(model, &iter, 1, &active, -1);
    g_print("***********************************wetoiquwerklfhnknk,nlkjl;k;k");

 //  gtk_label_set_text(label, active);
}


static GtkWidget *
create_view (void)
{
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;
    GtkWidget *view;

    view = gtk_tree_view_new();

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Buddies");
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,"text", COLUMN_BUDDIES);


    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Status");
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,"text", COLUMN_STATUS);

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Note");
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,"text", COLUMN_NOTE);

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Suscribed");
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,"text", COLUMN_SUBSCRIBED);

    return view;
}

void
update_buddylist_view(GtkWidget * view)
{
    if(!view)
        return; // Buddylist window not opend

    GtkTreeModel * model = create_and_fill_model();
    gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
    g_object_unref(model);
    g_debug("BuddyList updated.");
}



void
destroy_buddylist_window()
{
    g_debug("Destroy buddylist window ");
    presence_view_set(NULL);
    gtk_widget_destroy(buddylistwindow);
}

void
create_buddylist_window(SFLPhoneClient *client)
{
    const gchar * title = "SFLphone Buddies";
    g_debug("Create window : %s", title);

    buddylistwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_container_set_border_width(GTK_CONTAINER(buddylistwindow), 0);
    gtk_window_set_title(GTK_WINDOW(buddylistwindow), title);
    gtk_window_set_default_size(GTK_WINDOW(buddylistwindow), BUDDYLIST_WINDOW_WIDTH, BUDDYLIST_WINDOW_HEIGHT);
    gtk_widget_set_name(buddylistwindow, title);

    /* Instantiate vbox, subvbox as homogeneous */
    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);
    gtk_container_add(GTK_CONTAINER(buddylistwindow), vbox);

    /* Create the tree view*/
    GtkWidget *buddy_list_tree_view = create_view();
    gtk_box_pack_start(GTK_BOX(vbox), buddy_list_tree_view, TRUE, TRUE, 5);
    update_buddylist_view(buddy_list_tree_view);
    presence_view_set(buddy_list_tree_view);

    GtkTreeSelection *selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(buddy_list_tree_view));
    g_signal_connect(G_OBJECT(selection), "changed", G_CALLBACK(selection_changed), NULL);

    /* make sure that everything, window and label, are visible */
    gtk_widget_show_all(buddylistwindow);
}
