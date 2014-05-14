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
#include <string.h>
#include <glib/gi18n.h>

#include "mainwindow.h"
#include "presencewindow.h"
#include "account_schema.h"
#include "accountlist.h"
#include "actions.h"
#include "contacts/calltree.h"
#include "callable_obj.h"
#include "dbus.h"
#include "str_utils.h"
#include "statusicon.h"

static GtkWidget *presence_window;
static GtkTreeView *buddy_list_tree_view;
static GtkToggleAction *toggle_action;
static GtkWidget *presence_status_combo;
static GtkWidget *presence_status_bar;

static GtkTreeModel *create_and_fill_presence_tree(void);
static GtkTreeView *create_presence_view(void);
gboolean selection_changed(GtkTreeSelection *selection);
static gboolean presence_view_row_is_buddy(GtkTreeView *treeview, GtkTreePath *path);
static buddy_t *presence_view_row_get_buddy(GtkTreeView *treeview, GtkTreePath *path);
static gchar *presence_view_row_get_group(GtkTreeView *treeview, GtkTreePath *path);

static SFLPhoneClient *presence_client;
static buddy_t *tmp_buddy;
static gboolean show_all;

enum
{
    POPUP_MENU_TYPE_DEFAULT,
    POPUP_MENU_TYPE_BUDDY,
    POPUP_MENU_TYPE_GROUP
};

/***************************** tree view **********************************/

enum
{
    COLUMN_OVERVIEW,
    COLUMN_ALIAS,
    COLUMN_STATUS,
    COLUMN_NOTE,
    COLUMN_URI,
    COLUMN_SUBSCRIBED,
    COLUMN_ACCOUNTID,
    COLUMN_GROUP,
    N_COLUMN
};


/* User callback for "get"ing the data out of the row that was DnD'd */
void on_buddy_drag_data_get(GtkWidget *widget,
        G_GNUC_UNUSED GdkDragContext *drag_context,
        GtkSelectionData *sdata,
        G_GNUC_UNUSED guint info,
        G_GNUC_UNUSED guint time_,
        G_GNUC_UNUSED gpointer user_data)
{
    GtkTreeIter iter;
    GtkTreeModel *model;
    GtkTreeSelection *selector;

    /* Get the selector widget from the treeview in question */
    selector = gtk_tree_view_get_selection(GTK_TREE_VIEW(widget));

    /* Get the tree model (list_store) and initialise the iterator */
    if (!gtk_tree_selection_get_selected(selector, &model, &iter))
        return;

    // TODO : get the path would be cleaner
    model = gtk_tree_view_get_model(GTK_TREE_VIEW(widget));
    buddy_t *b = NULL;
    GValue val;
    memset(&val, 0, sizeof(val));
    gtk_tree_model_get_value(model, &iter, COLUMN_URI, &val);
    b = presence_buddy_list_buddy_get_by_uri(g_value_dup_string(&val));
    g_value_unset(&val);
    if (!b)
        return;

    g_debug("Drag src from buddy list b->uri : %s", b->uri);

    gtk_selection_data_set(sdata,
            gdk_atom_intern ("struct buddy_t pointer", FALSE),
            8,           // Tell GTK how to pack the data (bytes)
            (void *)&b,  // The actual pointer that we just made
            sizeof (b)); // The size of the pointer
}


void on_buddy_drag_data_received(GtkWidget *widget,
        G_GNUC_UNUSED GdkDragContext *drag_context,
        gint x, gint y, GtkSelectionData *sdata,
        G_GNUC_UNUSED guint info,
        G_GNUC_UNUSED guint time_,
        G_GNUC_UNUSED gpointer user_data)
{

    // GOAL: grab the "group" field from the target row (pointed on drop)
    // and apply it on the  dragged src buddy

    GtkTreePath *path;
    const guchar *data = gtk_selection_data_get_data(sdata);

    if (!gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), x, y, &path,
                NULL, NULL, NULL))
        return;

    //gchar * spath = gtk_tree_path_to_string(path);
    //g_debug("========= (%d,%d) => Path:%s\n", x, y, spath);
    gchar *gr_target = presence_view_row_get_group(GTK_TREE_VIEW(widget), path);
    gtk_tree_path_free(path);
    if (!gr_target) // set group to default
        gr_target = g_strdup(" ");

    buddy_t *b_src = NULL;
    memcpy(&b_src, data, sizeof(b_src));
    if (!b_src) {
        g_warning("Dragged src data not found");
        return;
    }

    buddy_t *b = presence_buddy_list_buddy_get_by_uri(b_src->uri);
    // the dragged source data refers to an existing buddy.
    if (b) {
        g_debug("Dragged src data found: %s, %s", b_src->alias, b_src->uri);
        buddy_t *backup = presence_buddy_copy(b);

        // set the group to the one under the mouse pointer
        g_free(b->group);
        b->group = g_strdup(gr_target);

        // The src may come from the history/contact window and the alias maybe change.
        // but if not, don't free b_src because b and b_src points to the same buddy.
        gchar *alias = g_strdup(b_src->alias);
        g_free(b->alias);
        b->alias = g_strdup(alias);
        presence_buddy_list_edit_buddy(b, backup);
        presence_buddy_delete(backup);
        // TODO change rank in tree view
    } else {
        // create the new buddy from the dragged source data
        g_free(b_src->group);
        b_src->group = g_strdup(gr_target);
        presence_buddy_list_add_buddy(b_src);
    }
    g_free(gr_target);
    update_presence_view();
}

static GtkTreeModel *
create_and_fill_presence_tree (void)
{
    GtkTreeStore *treestore;
    GtkTreeIter toplevel, child;
    GList * buddy_list = g_object_get_data(G_OBJECT(presence_window), "Buddy-List");
    buddy_t * buddy;

    treestore = gtk_tree_store_new(N_COLUMN, G_TYPE_STRING, // Group + photo
                                             G_TYPE_STRING, // Alias
                                             G_TYPE_STRING, // Group
                                             G_TYPE_STRING, // Status
                                             G_TYPE_STRING, // Note
                                             G_TYPE_STRING, // URI
                                             G_TYPE_STRING, // AccID
                                             G_TYPE_STRING);// subscribed

    // then display buddies with no group (==' ')
    for (guint j =  0; j < presence_buddy_list_get_size(buddy_list); j++) {
        buddy = presence_buddy_list_get_nth(j);
        account_t *acc = account_list_get_by_id(buddy->acc);
        if (acc == NULL)
            continue;

        if ((g_strcmp0(buddy->group, " ") == 0) &&
            ((g_strcmp0(buddy->note,"Not found") != 0) || show_all)) {
            gtk_tree_store_append(treestore, &toplevel, NULL);
            gtk_tree_store_set(treestore, &toplevel,
                    COLUMN_OVERVIEW, " ", // to be on the top of the sorted list
                    COLUMN_ALIAS, buddy->alias,
                    COLUMN_GROUP, buddy->group,
                    COLUMN_STATUS, (buddy->status)? GTK_STOCK_YES : GTK_STOCK_NO,
                    COLUMN_NOTE,  buddy->note,
                    COLUMN_URI,  buddy->uri,
                    COLUMN_ACCOUNTID, buddy->acc,
                    COLUMN_SUBSCRIBED, (buddy->subscribed)? "yes":"no",
                    -1);
        }
    }

    // then display the groups
    for (guint i = 0; i < presence_group_list_get_size(); i++) {
        gchar *group = presence_group_list_get_nth(i);
        gchar *tmp = g_markup_printf_escaped("<b>%s</b>", group);

        // display buddy with no group after
        if (g_strcmp0(group, " ") == 0)
            continue;

        gtk_tree_store_append(treestore, &toplevel, NULL);
        gtk_tree_store_set(treestore, &toplevel,
                COLUMN_OVERVIEW, tmp,
                COLUMN_ALIAS, "",
                COLUMN_GROUP, group,
                COLUMN_STATUS, "",
                COLUMN_NOTE, "",
                COLUMN_URI, "",
                COLUMN_ACCOUNTID, "",
                COLUMN_SUBSCRIBED, "",
                -1);
        g_free(tmp);

        for (guint j =  0; j < presence_buddy_list_get_size(buddy_list); j++) {
            buddy = presence_buddy_list_get_nth(j);
            account_t *acc = account_list_get_by_id(buddy->acc);
            if (acc == NULL)
                continue;

            if ((g_strcmp0(buddy->group, group) == 0) &&
               ((g_strcmp0(buddy->note,"Not found") != 0) || show_all)) {
                gtk_tree_store_append(treestore, &child, &toplevel);
                gtk_tree_store_set(treestore, &child,
                        COLUMN_OVERVIEW, "",
                        COLUMN_ALIAS, buddy->alias,
                        COLUMN_GROUP, buddy->group,
                        COLUMN_STATUS, (buddy->status)? GTK_STOCK_YES : GTK_STOCK_NO,
                        COLUMN_NOTE,  buddy->note,
                        COLUMN_URI,  buddy->uri,
                        COLUMN_ACCOUNTID, buddy->acc,
                        COLUMN_SUBSCRIBED, (buddy->subscribed) ? "yes" : "no",
                        -1);
            }
        }
    }

    // sort the groups and their buddies by name
    GtkTreeSortable * sortable = GTK_TREE_SORTABLE(treestore);
    gtk_tree_sortable_set_sort_column_id(sortable, COLUMN_OVERVIEW, GTK_SORT_ASCENDING);
    //gtk_tree_sortable_set_sort_column_id(sortable, COLUMN_ALIAS, GTK_SORT_ASCENDING);

    return GTK_TREE_MODEL(treestore);
}

void presence_view_cell_edited(G_GNUC_UNUSED GtkCellRendererText *renderer,
        gchar *path_str,
        gchar *new_text,
        GtkTreeView *treeview)
{
    GtkTreeViewColumn *focus_column;
    gtk_tree_view_get_cursor(treeview, NULL, &focus_column);
    const gchar * col_name = gtk_tree_view_column_get_title(focus_column);

    g_debug("Presence: value of col: %s is set to %s", col_name, new_text);
    GtkTreePath * path = gtk_tree_path_new_from_string(path_str);

    // a buddy line was edited
    if (presence_view_row_is_buddy(treeview, path)) {
        buddy_t *b = presence_view_row_get_buddy(treeview, path);
        if (!b)
            return;
        buddy_t *backup = presence_buddy_copy(b);

        if ((g_strcmp0(col_name, "Alias") == 0) &&
            (g_strcmp0(new_text, backup->alias) != 0)) {
            // alias was edited and has changed
            g_free(b->alias);
            b->alias = g_strdup(new_text);
            presence_buddy_list_edit_buddy(b, backup);
        } else if ((g_strcmp0(col_name, "URI") == 0) &&
                (g_strcmp0(new_text, backup->uri) != 0) &&
                (!presence_buddy_list_buddy_get_by_uri(new_text))) {
            // uri was edited and has changed to a new uri
            g_free(b->uri);
            b->uri = g_strdup(new_text);
            presence_buddy_list_edit_buddy(b, backup);
        }

        presence_buddy_delete(backup);
    } else {
        // a group line was edited
        gchar *group = presence_view_row_get_group(treeview, path);
        if (g_strcmp0(new_text, group) != 0)
            presence_group_list_edit_group(new_text, group);
        g_free(group);
    }

    gtk_tree_path_free(path);
    update_presence_view();
}

void cell_data_func(G_GNUC_UNUSED GtkTreeViewColumn *col,
        GtkCellRenderer   *renderer,
        GtkTreeModel      *model,
        GtkTreeIter       *iter,
        gpointer           userdata)
{
    guint col_ID = GPOINTER_TO_INT(userdata);
    // TODO: replace with get the path, get the group and get row_is_buddy
    GValue val;
    memset(&val, 0, sizeof(val));
    gtk_tree_model_get_value(model, iter, COLUMN_OVERVIEW, &val);
    gchar* group = g_value_dup_string(&val);
    g_value_unset(&val);

    // when the mouse pointer on a group field, set the cell editable
    // not a child buddy row and not an orphan buddy row
    if (g_strcmp0(group, "") != 0 && g_strcmp0(group, " ") != 0) {
        g_object_set(renderer, "editable", col_ID == COLUMN_OVERVIEW, NULL);
    } else {
        // set the cell editable when the mouse pointer is on a buddy alias or uri
        if (col_ID == COLUMN_ALIAS)
            g_object_set(renderer, "editable", TRUE, NULL);
        else if (col_ID == COLUMN_URI)
            g_object_set(renderer, "editable", TRUE, NULL);
        else
            g_object_set(renderer, "editable", FALSE, NULL);
    }
    g_free(group);
}

void icon_cell_data_func(G_GNUC_UNUSED GtkTreeViewColumn *col,
                           GtkCellRenderer   *renderer,
                           GtkTreeModel      *model,
                           GtkTreeIter       *iter,
                           G_GNUC_UNUSED gpointer userdata)
{
    GValue val;
    memset(&val, 0, sizeof(val));
    gtk_tree_model_get_value(model, iter, COLUMN_STATUS, &val);
    gchar *icon_name = g_value_dup_string(&val);
    g_value_unset(&val);
    g_object_set(renderer, "icon-name", icon_name, NULL);
}

void
update_presence_view()
{
    if (!buddy_list_tree_view)
        return; // presence window not opend

    GtkTreeModel * model = create_and_fill_presence_tree();
    gtk_tree_view_set_model(buddy_list_tree_view, model);
    g_object_unref(model);
    gtk_tree_view_expand_all(buddy_list_tree_view);
#ifdef PRESENCE_DEBUG
    g_debug("Presence: view updated.");
#endif
}

static GtkTreeView *
create_presence_view (void)
{
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new(); // default text render
    GtkWidget * view_widget = gtk_tree_view_new();
    GtkTreeView *view = GTK_TREE_VIEW(view_widget);

    GtkCellRenderer *editable_cell = gtk_cell_renderer_text_new();
    g_signal_connect(editable_cell, "edited",
            G_CALLBACK(presence_view_cell_edited), view);

    col = gtk_tree_view_column_new_with_attributes(" ",
            editable_cell, "markup", COLUMN_OVERVIEW, NULL);
    gtk_tree_view_append_column(view, col);
    // pack with the defaul renderer to avoid GTK warning.
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(col, editable_cell,
            cell_data_func, GINT_TO_POINTER(COLUMN_OVERVIEW), NULL);

    col = gtk_tree_view_column_new_with_attributes("Alias",
            editable_cell, "text", COLUMN_ALIAS, NULL);
    gtk_tree_view_append_column(view, col);
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(col, editable_cell,
            cell_data_func, GINT_TO_POINTER(COLUMN_ALIAS), NULL);

    GtkCellRenderer * status_icon_renderer  = gtk_cell_renderer_pixbuf_new();
    col = gtk_tree_view_column_new_with_attributes("Status",
            status_icon_renderer, "text", COLUMN_STATUS, NULL);
    // FIXME: set type to "text" instead of "pixbuf". this is a work around because
    // the gtk stock icon is referenced as a string BUT there is still a warning
    gtk_tree_view_append_column(view, col);
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(col, status_icon_renderer,
            icon_cell_data_func, GINT_TO_POINTER(COLUMN_STATUS), NULL);

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Note");
    gtk_tree_view_append_column(view, col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,"text", COLUMN_NOTE);

    col = gtk_tree_view_column_new_with_attributes("URI",
            editable_cell, "text", COLUMN_URI, NULL);
    gtk_tree_view_append_column(view, col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(col, editable_cell,
            cell_data_func, GINT_TO_POINTER(COLUMN_URI), NULL);
#ifndef PRESENCE_DEBUG
    gtk_tree_view_column_set_visible(col, FALSE);
#else
    gtk_tree_view_column_set_visible(col, TRUE);

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Suscribed");
    gtk_tree_view_append_column(view, col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,"text", COLUMN_SUBSCRIBED);

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "AccID");
    gtk_tree_view_append_column(view, col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,"text", COLUMN_ACCOUNTID);

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Group");
    gtk_tree_view_append_column(view, col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,"text", COLUMN_GROUP);
#endif

    return view;
}

/***************************** dialog win **********************************/


static gboolean
field_error_dialog(const gchar *error_string)
{
    gchar *msg;
    msg = g_markup_printf_escaped(_("Field error %s."), error_string);

    /* Create the widgets */
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(presence_window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_ERROR,
            GTK_BUTTONS_OK,
            "%s", msg);

    gtk_window_set_title(GTK_WINDOW(dialog), _("Field error"));

    const gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    gtk_widget_destroy(dialog);
    g_free(msg);

    return response == GTK_RESPONSE_OK;
}

gboolean
show_buddy_info_dialog(const gchar *title, buddy_t *b)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons((title),
                        GTK_WINDOW(presence_window),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_STOCK_CANCEL,
                        GTK_RESPONSE_CANCEL,
                        GTK_STOCK_APPLY,
                        GTK_RESPONSE_APPLY,
                        NULL);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
            grid, TRUE, TRUE, 0);

    gint row = 0;
    GtkWidget *label;

    // TODO: this is ugly but temporary
    guint group_index = 0;
    guint group_count = 0;
    GtkWidget * combo_group = NULL;
    if (g_strcmp0(title, "Add new buddy") != 0)
    {
        label = gtk_label_new_with_mnemonic("_Group");
        gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
        combo_group = gtk_combo_box_text_new();
        // fill combox with existing groups
        gchar *group;
        for (guint i = 0; i < presence_group_list_get_size(); i++) {
            group = presence_group_list_get_nth(i);
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_group), group);
            group_count++;
            // set active group
            if (g_strcmp0(group, b->group) == 0)
                group_index = i;
        }
        gtk_combo_box_set_active(GTK_COMBO_BOX(combo_group), (gint)group_index);

        gtk_grid_attach(GTK_GRID(grid), combo_group, 1, row, 1, 1);
        row++;
    }

    label = gtk_label_new_with_mnemonic("_Alias");
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    GtkWidget *entry_alias = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_alias);
    gtk_entry_set_text(GTK_ENTRY(entry_alias), b->alias);
    gtk_grid_attach(GTK_GRID(grid), entry_alias, 1, row, 1, 1);

    row++;
    label = gtk_label_new_with_mnemonic("_URI");
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    GtkWidget *entry_uri = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_uri);
    gtk_entry_set_text(GTK_ENTRY(entry_uri), b->uri);
    gtk_grid_attach(GTK_GRID(grid), entry_uri, 1, row, 1, 1);

    gtk_widget_show_all(grid);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    // update buddy OK was pressed
    if (response == GTK_RESPONSE_APPLY)
    {
        // this is ugly but temporary
        if (g_strcmp0(title, "Add new buddy") != 0)
        {
            // FIXME: this function doesn't work as expected
            // if(gtk_combo_box_get_has_entry(GTK_COMBO_BOX(combo_group)))
            if (group_count > 0) {
                gchar * gr = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo_group));
                g_free(b->group);
                b->group = g_strdup(gr);
                g_free(gr);
            }
        }
        g_free(b->alias);
        g_free(b->uri);
        b->alias = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry_alias)));
        b->uri = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry_uri)));

        // check the filed
        if (strlen(b->alias) == 0 || strlen(b->uri) == 0) {
            field_error_dialog("a field is empty");
            gtk_widget_destroy(dialog);
            gboolean res = show_buddy_info_dialog(title, b);// recursive call
            return res;
        }

        gtk_widget_destroy(dialog);
        return TRUE;
    }

    gtk_widget_destroy(dialog);
    return FALSE;
}


static gboolean
show_group_info_dialog(const gchar *title, gchar **group)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons((title),
                        GTK_WINDOW(presence_window),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_STOCK_CANCEL,
                        GTK_RESPONSE_CANCEL,
                        GTK_STOCK_APPLY,
                        GTK_RESPONSE_APPLY,
                        NULL);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
            grid, TRUE, TRUE, 0);

    gint row = 0;
    GtkWidget *label = gtk_label_new_with_mnemonic("Group");
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    GtkWidget *entry_group = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_group);
    gtk_entry_set_text(GTK_ENTRY(entry_group), *group);
    gtk_grid_attach(GTK_GRID(grid), entry_group, 1, row, 1, 1);

    gtk_widget_show_all(grid);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    // update buddy OK was pressed
    if (response == GTK_RESPONSE_APPLY)
    {
        g_free(*group);
        *group = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry_group)));
        gtk_widget_destroy(dialog);

        // check that the group name isn't empty or default
        if ((g_strcmp0(*group, "") == 0) || (g_strcmp0(*group," ") == 0))
            return FALSE;
        else
            return TRUE;
    }

    gtk_widget_destroy(dialog);
    return FALSE;
}


static gboolean
confirm_buddy_deletion(buddy_t *b)
{
    gchar *msg;
    msg = g_markup_printf_escaped(_("Are you sure want to delete \"%s\""), b->alias);

    /* Create the widgets */
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(presence_window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_CANCEL,
            "%s", msg);

    gtk_dialog_add_buttons(GTK_DIALOG(dialog), _("Remove"), GTK_RESPONSE_OK, NULL);
    gtk_window_set_title(GTK_WINDOW(dialog), _("Remove buddy"));

    const gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    gtk_widget_destroy(dialog);
    g_free(msg);

    return response == GTK_RESPONSE_OK;
}

static gboolean
confirm_group_deletion(gchar *group)
{
    gchar *msg;
    msg = g_markup_printf_escaped(_("Do you really want to delete the group %s"), group);

    /* Create the widgets */
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(presence_window),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_WARNING,
            GTK_BUTTONS_CANCEL,
            "%s", msg);

    gtk_dialog_add_buttons(GTK_DIALOG(dialog), _("Remove"), GTK_RESPONSE_OK, NULL);
    gtk_window_set_title(GTK_WINDOW(dialog), _("Remove buddy"));

    const gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    gtk_widget_destroy(dialog);
    g_free(msg);

    return response == GTK_RESPONSE_OK;
}

/*********************************  Contextual Menus ****************************/

static void
presence_view_popup_menu_onCallBuddy(G_GNUC_UNUSED GtkWidget *menuitem, gpointer userdata)
{
    buddy_t *b= (buddy_t*)userdata;
    callable_obj_t * c = sflphone_new_call(presence_client);

    g_free(c->_peer_number);
    c->_peer_number = g_strdup(b->uri);
    g_free(c->_accountID);
    c->_accountID = g_strdup(b->acc);

    calltree_update_call(current_calls_tab, c, presence_client);
    sflphone_place_call(c, presence_client);

    /* Legacy system tray option, requires TopIcons GNOME extension */
    status_tray_icon_blink(TRUE);
    if (g_settings_get_boolean(presence_client->settings, "popup-main-window"))
        popup_main_window(presence_client);

    if (g_settings_get_boolean(presence_client->settings, "bring-window-to-front"))
        main_window_bring_to_front(presence_client, c->_time_start);
}

static void
presence_view_popup_menu_onAddBuddy(G_GNUC_UNUSED GtkWidget *menuitem, gpointer userdata)
{
    buddy_t *b = presence_buddy_create();

    // try to grab the selected group
    if (userdata != NULL) {
        // might be a group row or a buddy row
        buddy_t *tmp_b = ((buddy_t*)userdata);
        g_free(b->group);
        b->group = g_strdup(tmp_b->group);
    }

    account_t *acc = account_list_get_current();
    if (!acc) {
        acc = account_list_get_nth(1); //0 is IP2IP
        if (!acc) {
            g_warning("At least one account must exist to able to subscribe.");
            return;
        }
    }
    g_free(b->acc);
    b->acc = g_strdup(acc->accountID);

    gchar * uri = g_strconcat("<sip:XXXX@", account_lookup(acc, CONFIG_ACCOUNT_HOSTNAME), ">", NULL);
    g_free(b->uri);
    b->uri = g_strdup(uri);
    g_free(uri);

    if (show_buddy_info_dialog(_("Add new buddy"), b)) {
        presence_buddy_list_add_buddy(b);
        update_presence_view();
    } else {
        presence_buddy_delete(b);
    }
}

static void
presence_view_popup_menu_onRemoveBuddy (G_GNUC_UNUSED GtkWidget *menuitem, gpointer userdata)
{
    buddy_t *b = (buddy_t*) userdata;
    if (confirm_buddy_deletion(b)) {
        presence_buddy_list_remove_buddy(b);
        update_presence_view();
    }
}

static void
presence_view_popup_menu_onAddGroup (G_GNUC_UNUSED GtkWidget *menuitem, G_GNUC_UNUSED gpointer userdata)
{
    gchar *group = g_strdup("");
    if (show_group_info_dialog(_("Add group"), &group)) {
        presence_group_list_add_group(group);
        update_presence_view();
    }
    g_free(group);
}

static void
presence_view_popup_menu_onRemoveGroup (G_GNUC_UNUSED GtkWidget *menuitem, gpointer userdata)
{
    gchar *group = (gchar*) userdata;
    if (confirm_group_deletion(group)) {
        presence_group_list_remove_group(group);
        update_presence_view();
    }
}

static void
presence_view_popup_menu(GdkEventButton *event, gpointer userdata, guint type)
{
    GtkWidget *menu, *menuitem;
    menu = gtk_menu_new(); //TODO free ???
    gchar* gr = g_strdup(((buddy_t*)userdata)->group); // TODO; is it necessary to be free?

    if (type == POPUP_MENU_TYPE_BUDDY) {
        menuitem = gtk_menu_item_new_with_label(_("Call"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
        g_signal_connect(menuitem, "activate",
                G_CALLBACK(presence_view_popup_menu_onCallBuddy), userdata);

        menuitem = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    }

    menuitem = gtk_menu_item_new_with_label(_("Add buddy"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    g_signal_connect(menuitem, "activate",
            G_CALLBACK(presence_view_popup_menu_onAddBuddy), userdata);

    if (type == POPUP_MENU_TYPE_BUDDY) {
        menuitem = gtk_menu_item_new_with_label(_("Remove buddy"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
        g_signal_connect(menuitem, "activate",
                G_CALLBACK(presence_view_popup_menu_onRemoveBuddy), userdata);
    }

    menuitem = gtk_separator_menu_item_new(); // TODO:free?
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    menuitem = gtk_menu_item_new_with_label(_("Add group"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    g_signal_connect(menuitem, "activate",
            G_CALLBACK(presence_view_popup_menu_onAddGroup), gr);

    if (type == POPUP_MENU_TYPE_GROUP) {
        menuitem = gtk_menu_item_new_with_label(_("Remove group"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
        g_signal_connect(menuitem, "activate",
                G_CALLBACK(presence_view_popup_menu_onRemoveGroup), gr);
    }

    gtk_widget_show_all(menu);

    /* Note: event can be NULL here when called from view_onPopupMenu;
     *  gdk_event_get_time() accepts a NULL argument */
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
            (event != NULL) ? event->button : 0,
            gdk_event_get_time((GdkEvent*)event));
}

static gboolean
presence_view_row_is_buddy(GtkTreeView *treeview, GtkTreePath *path)
{
    GtkTreeIter   iter;
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);
    gboolean res = FALSE;

    if (gtk_tree_model_get_iter(model, &iter, path)) {
        GValue val;
        memset(&val, 0, sizeof(val));
        gtk_tree_model_get_value(model, &iter, COLUMN_URI, &val);
        gchar *uri = g_value_dup_string(&val);
        res = strlen(uri) > 0;
        g_value_unset(&val);
        g_free(uri);
    }
    return res;
}

static buddy_t *
presence_view_row_get_buddy(GtkTreeView *treeview, GtkTreePath *path)
{
    GtkTreeSelection * selection = gtk_tree_view_get_selection(treeview);
    buddy_t *b = NULL;
    gtk_tree_selection_unselect_all(selection);
    gtk_tree_selection_select_path(selection, path);

    GtkTreeIter   iter;
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);

    if (gtk_tree_model_get_iter(model, &iter, path)) {
        GValue val;
        memset(&val, 0, sizeof(val));
        gtk_tree_model_get_value(model, &iter, COLUMN_URI, &val);
        gchar *uri = g_value_dup_string(&val);
        b = presence_buddy_list_buddy_get_by_uri(uri);
        g_value_unset(&val);
        g_free(uri);
    }
    g_assert(b);
    return b;
}

static gchar *
presence_view_row_get_group(GtkTreeView *treeview, GtkTreePath *path)
{
    GtkTreeSelection * selection = gtk_tree_view_get_selection(treeview);
    gchar *group = NULL;
    gtk_tree_selection_unselect_all(selection);
    gtk_tree_selection_select_path(selection, path);

    GtkTreeIter   iter;
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);

    if (gtk_tree_model_get_iter(model, &iter, path)) {
        GValue val;
        memset(&val, 0, sizeof(val));
        gtk_tree_model_get_value(model, &iter, COLUMN_GROUP, &val);
        group = g_value_dup_string(&val);
        g_value_unset(&val);
    }
    g_assert(group);
    return group;
}

static gboolean
presence_view_onButtonPressed(GtkTreeView *treeview, GdkEventButton *event)
{
    /* single click with the right mouse button? */
    if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
        GtkTreeSelection * selection = gtk_tree_view_get_selection(treeview);

        /* Note: gtk_tree_selection_count_selected_rows() does not
         *   exist in gtk+-2.0, only in gtk+ >= v2.2 ! */
        if (gtk_tree_selection_count_selected_rows(selection) == 1) {
            GtkTreePath *path;
            /* Get tree path for row that was clicked */
            if (gtk_tree_view_get_path_at_pos(treeview,
                        (gint) event->x,
                        (gint) event->y,
                        &path, NULL, NULL, NULL)) {
                if (presence_view_row_is_buddy(treeview, path)) {
                    buddy_t *b = presence_view_row_get_buddy(treeview, path);
                    if (b)
                        presence_view_popup_menu(event, b, POPUP_MENU_TYPE_BUDDY);
                } else {
                    // a group row has been selected
                    // use a fake buddy as argument
                    g_free(tmp_buddy->group);
                    tmp_buddy->group = g_strdup(presence_view_row_get_group(treeview, path));
                    if (tmp_buddy->group != NULL)
                        presence_view_popup_menu(event, tmp_buddy, POPUP_MENU_TYPE_GROUP);
                }
                gtk_tree_path_free(path);
                return TRUE;
            }
        }
        //else right click on the back ground
        // use a fake buddy as argument
        g_free(tmp_buddy->group);
        tmp_buddy->group = g_strdup(" "); // default group
        presence_view_popup_menu(event, tmp_buddy, POPUP_MENU_TYPE_DEFAULT);

        return TRUE;
    }
    return FALSE;
}

/*
static void
view_row_activated_cb(GtkTreeView *treeview,
        GtkTreePath *path,
        G_GNUC_UNUSED GtkTreeViewColumn *col)
{
    buddy_t *b = view_get_buddy(treeview, path);
    if(b) //"NULL if an group was selected instead of a buddy.
        presence_view_popup_menu_onEditBuddy(NULL, b);
}
*/

/******************************** Status bar *********************************/

/**
 * This function reads the status combo box and call the DBus presence
 * publish method if enabled.
 * @param combo The text combo box associated with the status to be published.
 */
static void
presence_statusbar_changed_cb(GtkComboBox *combo)
{
    const gchar *status = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
    gboolean b = g_strcmp0(status, PRESENCE_STATUS_ONLINE) == 0;
    account_t * account;

    //send publish to every registered accounts with presence and publish enabled
    for (guint i = 0; i < account_list_get_size(); i++) {
        account = account_list_get_nth(i);
        g_assert(account);
        account_replace(account, CONFIG_PRESENCE_STATUS, status);

        if ((g_strcmp0(account_lookup(account, CONFIG_PRESENCE_PUBLISH_SUPPORTED), "true") == 0) &&
            (g_strcmp0(account_lookup(account, CONFIG_PRESENCE_ENABLED), "true") == 0) &&
            (((g_strcmp0(account_lookup(account, CONFIG_ACCOUNT_ENABLE), "true") == 0) ||
            (account_is_IP2IP(account))))) {
            dbus_presence_publish(account->accountID, b);
            g_debug("Presence: publish status of acc:%s => %s", account->accountID, status);
        }
    }
}

void
update_presence_statusbar()
{
    account_t * account;
    gboolean global_publish_enabled = FALSE;
    gboolean global_status = FALSE;

    if (!presence_status_bar) // presence window not opened
        return;

    /* Check if one of the registered accounts has Presence enabled */
    for (guint i = 0; i < account_list_get_size(); i++) {
        account = account_list_get_nth(i);
        g_assert(account);

        if (!(account_is_IP2IP(account)) &&
                (g_strcmp0(account_lookup(account, CONFIG_PRESENCE_ENABLED), "true") == 0) &&
                (account->state == ACCOUNT_STATE_REGISTERED))
        {
            if (g_strcmp0(account_lookup(account, CONFIG_PRESENCE_STATUS), PRESENCE_STATUS_ONLINE) == 0)
                global_status = TRUE;

            if (g_strcmp0(account_lookup(account, CONFIG_PRESENCE_PUBLISH_SUPPORTED), "true") == 0)
                global_publish_enabled = TRUE;
        }
    }

    if (global_status)
        g_debug("Presence: statusbar, at least 1 account is registered.");
    if (global_publish_enabled)
        g_debug("Presence: statusbar, at least 1 account can publish.");

    gtk_combo_box_set_active(GTK_COMBO_BOX(presence_status_combo),  global_status? 1:0);
    gtk_widget_set_sensitive(presence_status_combo, global_publish_enabled? TRUE:FALSE);
}

GtkWidget*
create_presence_statusbar()
{
    GtkWidget *bar = gtk_statusbar_new();

    GtkWidget *label = gtk_label_new_with_mnemonic(_("Status:"));
    gtk_box_pack_start(GTK_BOX(bar), label, TRUE, TRUE, 0);

    /* Add presence status combo_box*/
    presence_status_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(presence_status_combo),
            _(PRESENCE_STATUS_OFFLINE));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(presence_status_combo),
            _(PRESENCE_STATUS_ONLINE));
    gtk_widget_set_sensitive(presence_status_combo, TRUE);
    gtk_box_pack_start(GTK_BOX(bar), presence_status_combo, TRUE, TRUE, 0);

    g_signal_connect(G_OBJECT(presence_status_combo), "changed",
            G_CALLBACK(presence_statusbar_changed_cb), NULL);
    //update_presence_statusbar();

    return bar;
}


/*************************** Menu bar callback *******************************/

void
file_on_addbuddy_cb(GtkWidget *w)
{
    presence_view_popup_menu_onAddBuddy(w, NULL);
}

void
file_on_addgroup_cb(GtkWidget *w)
{
    presence_view_popup_menu_onAddGroup(w, NULL);
}

void
view_allbuddies_toggled_cb(GtkWidget *toggle)
{
    /* Display all the buddies including 'Not found' ones.*/
    show_all = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(toggle));
    update_presence_view();
}

void
view_uri_toggled_cb(GtkWidget *toggle)
{
    // show or hide the uri column
    GtkTreeViewColumn *col = gtk_tree_view_get_column(buddy_list_tree_view, (gint)COLUMN_URI);

    gtk_tree_view_column_set_visible(col,
            gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(toggle)));

    update_presence_view();
}

GtkWidget *
create_presence_menubar()
{
    GtkWidget *menu_bar = gtk_menu_bar_new();
    GtkWidget *item_file = gtk_menu_item_new_with_label("File");
    GtkWidget *item_view = gtk_menu_item_new_with_label("View");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), item_file);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu_bar), item_view);

    GtkWidget *menu;
    GtkWidget *menu_item;

    menu = gtk_menu_new();
    menu_item = gtk_menu_item_new_with_label("Add buddy...");
    g_signal_connect(G_OBJECT(menu_item), "activate",
            G_CALLBACK(file_on_addbuddy_cb), menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    menu_item = gtk_menu_item_new_with_label("Add group...");
    g_signal_connect(G_OBJECT(menu_item), "activate",
            G_CALLBACK(file_on_addgroup_cb), menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_file), menu);
    menu_item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    menu_item = gtk_menu_item_new_with_label("Close");
    g_signal_connect(G_OBJECT(menu_item), "activate",
            G_CALLBACK(destroy_presence_window), NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);


    menu = gtk_menu_new();
    menu_item = gtk_check_menu_item_new_with_label("All buddies");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), FALSE);
    g_signal_connect(G_OBJECT(menu_item), "activate",
            G_CALLBACK(view_allbuddies_toggled_cb), menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    menu_item = gtk_check_menu_item_new_with_label("Editable URIs");
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_item), FALSE);
    g_signal_connect(G_OBJECT(menu_item), "activate",
            G_CALLBACK(view_uri_toggled_cb), menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(item_view), menu);

    return menu_bar;
}

/******************************** window  *********************************/
gboolean
buddy_subsribe_timer_cb()
{
    presence_buddy_list_init(presence_client);
    return TRUE; // this is necessary to keep the timer alive
}

void
destroy_presence_window()
{
    g_debug("Destroy presence window ");
    buddy_list_tree_view = NULL;
    presence_status_bar = NULL;
    gtk_widget_destroy(presence_window);

    gtk_toggle_action_set_active(toggle_action, FALSE);
    //presence_buddy_list_flush();
}

void
create_presence_window(SFLPhoneClient *client, GtkToggleAction *action)
{
    static const int PRESENCE_WINDOW_WIDTH = 280;
    static const int PRESENCE_WINDOW_HEIGHT = 320;

    /* keep track of the widget which opened that window and the SFL client */
    toggle_action = action;
    presence_client = client;

    const gchar * title = _("SFLphone Presence");
    g_debug("Create window : %s", title);

    /*--------------------- Window -------------------------------------*/
    presence_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width(GTK_CONTAINER(presence_window), 0);
    gtk_window_set_title(GTK_WINDOW(presence_window), title);
    gtk_window_set_default_size(GTK_WINDOW(presence_window),
            PRESENCE_WINDOW_WIDTH, PRESENCE_WINDOW_HEIGHT);
    gtk_widget_set_name(presence_window, title);

    /*---------------- Instantiate vbox as homogeneous ----------------*/
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);
    gtk_container_add(GTK_CONTAINER(presence_window), vbox);

    /*----------------------- Create the menu bar----------------------*/
    GtkWidget * menu_bar = create_presence_menubar();
    gtk_box_pack_start(GTK_BOX(vbox), menu_bar, FALSE, FALSE, 3);

    /*---------------------- Create the tree view ---------------------*/
    buddy_list_tree_view = create_presence_view();
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(buddy_list_tree_view), TRUE, TRUE, 5);
    gtk_tree_view_set_reorderable(buddy_list_tree_view, TRUE);
    gtk_tree_view_set_rules_hint(buddy_list_tree_view,TRUE);
    gtk_tree_view_set_headers_visible(buddy_list_tree_view, FALSE);

    /*---------------------- Drag-n-Drop ------------------------------*/
    //DnD, the treeview is one the drag sources to drop buddies into groups
    gtk_drag_source_set(GTK_WIDGET(buddy_list_tree_view), GDK_BUTTON1_MASK,
            &presence_drag_targets, 1, GDK_ACTION_COPY|GDK_ACTION_MOVE);
    g_signal_connect(GTK_WIDGET(buddy_list_tree_view), "drag-data-get",
            G_CALLBACK(on_buddy_drag_data_get), NULL);
    gtk_drag_dest_set(GTK_WIDGET(buddy_list_tree_view), GTK_DEST_DEFAULT_ALL,
         &presence_drag_targets, 1, GDK_ACTION_COPY|GDK_ACTION_MOVE);
    g_signal_connect(GTK_WIDGET(buddy_list_tree_view), "drag-data-received",
            G_CALLBACK(on_buddy_drag_data_received), NULL);

    /*-------------------------- Status bar--------------------------- */
    presence_status_bar = create_presence_statusbar();
    gtk_box_pack_start(GTK_BOX(vbox), presence_status_bar, FALSE, TRUE, 0);

    g_signal_connect(G_OBJECT(buddy_list_tree_view), "button-press-event",
            G_CALLBACK(presence_view_onButtonPressed), NULL);
    g_signal_connect(G_OBJECT(presence_window), "button-press-event",
            G_CALLBACK(presence_view_onButtonPressed), NULL);
    g_signal_connect_after(presence_window, "destroy",
            G_CALLBACK(destroy_presence_window), NULL);

    /*------------------------- Load presence -------------------------*/
    presence_buddy_list_init(presence_client);
    g_object_set_data(G_OBJECT(presence_window), "Buddy-List",
            (gpointer) presence_buddy_list_get());
    update_presence_view();
    tmp_buddy = presence_buddy_create();

    /*------------------ Timer to refresh the subscription------------ */
    g_timeout_add_seconds(120, (GSourceFunc) buddy_subsribe_timer_cb, NULL);

    /* make sure that everything, window and label, are visible */
    gtk_widget_show_all(presence_window);
}
