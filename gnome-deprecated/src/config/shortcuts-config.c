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

#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <X11/XKBlib.h>
#include "shortcuts-config.h"
#include "shortcuts.h"


static void
accel_cleared(G_GNUC_UNUSED GtkCellRendererAccel *renderer, gchar *path,
              GtkTreeView *treeview)
{
    // Update treeview
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);

    GtkTreeIter iter;

    if (gtk_tree_model_get_iter_from_string(model, &iter, path))
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, MASK, 0, VALUE, 0, -1);

    // Update GDK bindings
    shortcuts_update_bindings(atoi(path), 0, 0);
}

static void
accel_edited(G_GNUC_UNUSED GtkCellRendererAccel *renderer, gchar *path, guint accel_key,
             GdkModifierType mask, G_GNUC_UNUSED guint hardware_keycode, GtkTreeView *treeview)
{
    // Disable existing binding if key already used
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    GtkTreeIter iter;
    gtk_tree_model_get_iter_first(model, &iter);

    Accelerator* list = shortcuts_get_list();
    const guint code = XKeysymToKeycode(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), accel_key);

    for (guint i = 0; list[i].action != NULL; ++i) {
        if (list[i].key == code && list[i].mask == mask) {
            gtk_list_store_set(GTK_LIST_STORE(model), &iter, MASK, 0, VALUE, 0,
                               -1);
            g_warning("This key was already affected");
        }

        if (!gtk_tree_model_iter_next(model, &iter))
            break;
    }

    // Update treeview
    if (gtk_tree_model_get_iter_from_string(model, &iter, path))
        gtk_list_store_set(GTK_LIST_STORE(model), &iter, MASK, (gint) mask,
                           VALUE, accel_key, -1);

    // Update GDK bindings
    shortcuts_update_bindings(atoi(path), code, mask);
}

/*
 * Create a tree view with two columns. The first is an action and the
 * second is a keyboard accelerator.
 */
static void
setup_tree_view(GtkWidget *treeview)
{
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new();
    GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes("Action", renderer,
                                "text", ACTION, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);

    renderer = gtk_cell_renderer_accel_new();
    g_object_set(renderer, "accel-mode", GTK_CELL_RENDERER_ACCEL_MODE_GTK,
                 "editable", TRUE, NULL);
    column = gtk_tree_view_column_new_with_attributes("Shortcut", renderer,
             "accel-mods", MASK, "accel-key", VALUE, NULL);

    gtk_tree_view_append_column(GTK_TREE_VIEW(treeview), column);
    g_signal_connect(renderer, "accel_edited", G_CALLBACK(accel_edited),
                     (gpointer) treeview);
    g_signal_connect(renderer, "accel_cleared", G_CALLBACK(accel_cleared),
                     (gpointer) treeview);
}

GtkWidget*
create_shortcuts_settings()
{
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    GtkWidget * result_frame = gnome_main_section_new(_("General"));

    GtkWidget *label = gtk_label_new(_("Be careful: these shortcuts might "
                                       "override system-wide shortcuts."));
    GtkWidget *treeview = gtk_tree_view_new();
    setup_tree_view(treeview);

    GtkListStore *store = gtk_list_store_new(COLUMNS, G_TYPE_STRING, G_TYPE_INT,
                          G_TYPE_UINT);

    Accelerator* list = shortcuts_get_list();

    for (guint i = 0; list[i].action != NULL; ++i) {
        GtkTreeIter iter;
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter, ACTION, _(list[i].action), MASK,
                           (gint) list[i].mask, VALUE,
                           XkbKeycodeToKeysym(GDK_DISPLAY_XDISPLAY(gdk_display_get_default()), list[i].key, 0, 0), -1);
    }

    gtk_tree_view_set_model(GTK_TREE_VIEW(treeview), GTK_TREE_MODEL(store));
    g_object_unref(store);

    gtk_container_add(GTK_CONTAINER(result_frame), treeview);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), result_frame, FALSE, FALSE, 0);

    gtk_widget_show_all(vbox);

    return vbox;
}

