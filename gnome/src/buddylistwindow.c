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

enum
{
    BUDDIES_COLUMN,
    STATUS_COLUMN,
    SUBSCRIBED_COLUMN
 // NUM_COLS = 2
};

//#define STATUS_COLUMN 1


static GtkTreeModel *
create_and_fill_model (void)
{
    GtkTreeStore *treestore;
    GtkTreeIter toplevel, child;
    treestore = gtk_tree_store_new(2, G_TYPE_STRING, G_TYPE_STRING);

    GList * buddy_list = presence_get_list();
    buddy_t * buddy;

    for (guint i = 0; i < account_list_get_size(); i++)
    {
        account_t * acc = account_list_get_nth(i);
        if (acc->state == (ACCOUNT_STATE_REGISTERED))
        {
            gtk_tree_store_append(treestore, &toplevel, NULL);
            gtk_tree_store_set(treestore, &toplevel,
                    BUDDIES_COLUMN, (gchar*) account_lookup(acc, CONFIG_ACCOUNT_ALIAS),
                    STATUS_COLUMN, "",
                    -1);
            for (guint j =  1; j < presence_list_get_size(buddy_list); j++)
            {
                buddy = presence_list_get_nth(buddy_list, j);
                if(g_strcmp0(buddy->acc, (gchar*)account_lookup(acc, CONFIG_ACCOUNT_ID))==0)
                {
                    gtk_tree_store_append(treestore, &child, &toplevel);
                    gtk_tree_store_set(treestore, &child,
                        BUDDIES_COLUMN, buddy->uri,
                        STATUS_COLUMN, buddy->status? "Online":"Offline",
                        SUBSCRIBED_COLUMN, buddy->subscribed? "Active":"Inactive",
                        -1);
                }
            }
        }
    }
    return GTK_TREE_MODEL(treestore);
}



static GtkWidget *
create_view_and_model (void)
{
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;
    GtkWidget *view;
    GtkTreeModel *model;

    view = gtk_tree_view_new();

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Buddies");
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,"text", BUDDIES_COLUMN);


    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Status");
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,"text", STATUS_COLUMN);

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Suscribed");
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,"text", STATUS_COLUMN);

    model = create_and_fill_model();
    gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);
    g_object_unref(model);

    return view;
}


void
destroy_buddylist_window()

{
    g_debug("Destroy buddylist window ");
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




    GtkWidget *list = create_view_and_model();//gtk_tree_view_new();
    gtk_box_pack_start(GTK_BOX(vbox), list, TRUE, TRUE, 5);

    /* make sure that everything, window and label, are visible */
    gtk_widget_show_all(buddylistwindow);
}
