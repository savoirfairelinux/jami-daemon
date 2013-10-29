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

#include "buddylistwindow.h"
#include "account_schema.h"
#include "accountlist.h"
#include "actions.h"
#include "contacts/calltree.h"
#include "callable_obj.h"
#include "dbus.h"
#include "str_utils.h"

static GtkWidget *buddy_list_window;
static GtkTreeView *buddy_list_tree_view = NULL;
static GtkToggleAction *toggle_action = NULL;
static GtkWidget *presence_status_combo;
static GtkWidget *presence_status_bar = NULL;
static GtkWidget *show_all_check_box;

static GtkTreeModel *create_and_fill_buddylist_tree (void);
static GtkTreeView *create_view (void);
gboolean selection_changed(GtkTreeSelection *selection);
static buddy_t *view_get_buddy(GtkTreeView *treeview, GtkTreePath *path);
static gchar *view_get_group(GtkTreeView *treeview, GtkTreePath *path);
void update_buddylist_view();

static SFLPhoneClient * presence_client;
static buddy_t * tmp_buddy;

enum
{
    POPUP_MENU_TYPE_DEFAULT = 0,
    POPUP_MENU_TYPE_BUDDY = 1,
    POPUP_MENU_TYPE_GROUP = 2,
};

/***************************** tree view **********************************/

enum
{
    COLUMN_OVERVIEW,
    COLUMN_ALIAS,
    COLUMN_GROUP,
    COLUMN_STATUS,
    COLUMN_NOTE,
    COLUMN_URI,
    COLUMN_ACCOUNTID,
    COLUMN_SUBSCRIBED
};

#define N_COLUMN 8

static GtkTreeModel *
create_and_fill_buddylist_tree (void)
{
    GtkTreeStore *treestore;
    GtkTreeIter toplevel, child;
    GList * buddy_list = g_object_get_data(G_OBJECT(buddy_list_window), "Buddy-List");
    buddy_t * buddy;
    gboolean show_all= gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(show_all_check_box));

    treestore = gtk_tree_store_new(N_COLUMN, G_TYPE_STRING, // Group + photo
                                             G_TYPE_STRING, // Alias
                                             G_TYPE_STRING, // Group
                                             G_TYPE_STRING, // Status
                                             G_TYPE_STRING, // Note
                                             G_TYPE_STRING, // URI
                                             G_TYPE_STRING, // AccID
                                             G_TYPE_STRING);// subscribed

    // then display buddies with no group (==' ')
    for (guint j =  1; j < presence_buddy_list_get_size(buddy_list); j++)
    {
        buddy = presence_buddy_list_get_nth(j);
        account_t *acc = account_list_get_by_id(buddy->acc);
        if(acc == NULL)
            continue;

        if((g_strcmp0(buddy->group, " ")==0) &&
                ((acc->state == ACCOUNT_STATE_REGISTERED) || show_all))
        {
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
    for (guint i = 1; i < presence_group_list_get_size(); i++)
    {
        gchar *group = presence_group_list_get_nth(i);

        // display buddy with no group after
        if(g_strcmp0(group, " ")==0)
            continue;

        gtk_tree_store_append(treestore, &toplevel, NULL);
        gtk_tree_store_set(treestore, &toplevel,
                COLUMN_OVERVIEW, group,
                COLUMN_ALIAS, "",
                COLUMN_GROUP, group,
                COLUMN_STATUS, "",
                COLUMN_NOTE, "",
                COLUMN_URI, "",
                COLUMN_ACCOUNTID, "",
                COLUMN_SUBSCRIBED, "",
                -1);

        for (guint j =  1; j < presence_buddy_list_get_size(buddy_list); j++)
        {
            buddy = presence_buddy_list_get_nth(j);
            account_t *acc = account_list_get_by_id(buddy->acc);
            if(acc == NULL)
                continue;

            if((g_strcmp0(buddy->group, group)==0) &&
                    ((acc->state == ACCOUNT_STATE_REGISTERED) || show_all))
            {
                gtk_tree_store_append(treestore, &child, &toplevel);
                gtk_tree_store_set(treestore, &child,
                        COLUMN_OVERVIEW, "",
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
    }

    // sort the groups and their buddies by name
    GtkTreeSortable * sortable = GTK_TREE_SORTABLE(treestore);
    gtk_tree_sortable_set_sort_column_id(sortable, COLUMN_OVERVIEW, GTK_SORT_ASCENDING);
    //gtk_tree_sortable_set_sort_column_id(sortable, COLUMN_ALIAS, GTK_SORT_ASCENDING);

    return GTK_TREE_MODEL(treestore);
}

void cell_edited(G_GNUC_UNUSED GtkCellRendererText *renderer,
        gchar *path_str,
        gchar *new_text,
        GtkTreeView *treeview)
{
    g_print("Cell new text: %s", new_text);
    GtkTreePath * path = gtk_tree_path_new_from_string(path_str);
    buddy_t *b = view_get_buddy(treeview, path);

    if(b != NULL) // change the buddy alias
    {
        buddy_t *backup = g_malloc(sizeof(buddy_t));
        memcpy(backup, b, sizeof(buddy_t));
        g_free(b->alias);
        b->alias = g_strdup(new_text);
        presence_buddy_list_edit_buddy(b, backup);
        g_free(backup);
    }
    else    // or change the group name
    {
        gchar *group = view_get_group(treeview, path);
        if (group == NULL)
            return;
        presence_group_list_edit_group(new_text, group);
    }
    update_buddylist_view();
}

void cell_data_func(G_GNUC_UNUSED GtkTreeViewColumn *col,
                           GtkCellRenderer   *renderer,
                           GtkTreeModel      *model,
                           GtkTreeIter       *iter,
                           gpointer           userdata)
{
    guint col_ID = GPOINTER_TO_INT(userdata);
    gchar * group = NULL;
    GValue val;
    memset(&val, 0, sizeof(val));
    gtk_tree_model_get_value(model, iter, COLUMN_OVERVIEW, &val);
    group = g_value_dup_string(&val);
    g_value_unset(&val);
    //g_print("/// %d: %s\n", col_ID, group);

    // set the cell editable when the mouse pointer is on a group
    if((group) && (g_strcmp0(group,"")!=0) && (g_strcmp0(group," ")))
    {
        g_object_set(renderer, "editable", (col_ID==COLUMN_OVERVIEW)? TRUE:FALSE, NULL);
    }
    // set the cell editable when the mouse pointer is on a buddy alias
    else
    {
        g_object_set(renderer, "editable", (col_ID==COLUMN_ALIAS)? TRUE:FALSE, NULL);
    }
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
update_buddylist_view()
{
    if(!buddy_list_tree_view)
        return; // Buddylist window not opend

    GtkTreeModel * model = create_and_fill_buddylist_tree();
    gtk_tree_view_set_model(buddy_list_tree_view, model);
    g_object_unref(model);
    gtk_tree_view_expand_all(buddy_list_tree_view);
    g_debug("BuddyListTreeView updated.");
}

static GtkTreeView *
create_view (void)
{
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer = gtk_cell_renderer_text_new(); // default text render
    GtkWidget * view_widget = gtk_tree_view_new();
    GtkTreeView *view = GTK_TREE_VIEW(view_widget);

    GtkCellRenderer *editable_cell = gtk_cell_renderer_text_new ();
    g_signal_connect (editable_cell, "edited",G_CALLBACK(cell_edited), view);

    col = gtk_tree_view_column_new_with_attributes(" ",
            editable_cell, "text", COLUMN_OVERVIEW, NULL);
    gtk_tree_view_append_column(view, col);
    gtk_tree_view_column_pack_start(col, editable_cell, TRUE);
    gtk_tree_view_column_set_cell_data_func(col, editable_cell,
            cell_data_func, GINT_TO_POINTER(COLUMN_OVERVIEW), NULL);

    col = gtk_tree_view_column_new_with_attributes("Alias",
            editable_cell, "text", COLUMN_ALIAS, NULL);
    gtk_tree_view_append_column(view, col);
    gtk_tree_view_column_pack_start(col, editable_cell, TRUE);
    gtk_tree_view_column_set_cell_data_func(col, editable_cell,
            cell_data_func, GINT_TO_POINTER(COLUMN_ALIAS), NULL);

    GtkCellRenderer * status_icon_renderer  = gtk_cell_renderer_pixbuf_new();
    col = gtk_tree_view_column_new_with_attributes("Status",
            status_icon_renderer, "text", COLUMN_STATUS, NULL);
    // set type to "text" instead of "pixbuf". this is a work around because
    // the gtk stock icon is referenced as a string
    gtk_tree_view_append_column(view, col);
    gtk_tree_view_column_pack_start(col, status_icon_renderer, TRUE);
    gtk_tree_view_column_set_cell_data_func(col, status_icon_renderer,
            icon_cell_data_func, GINT_TO_POINTER(COLUMN_STATUS), NULL);

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Note");
    gtk_tree_view_append_column(view, col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,"text", COLUMN_NOTE);

#ifdef PRESENCE_DEBUG
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "URI");
    gtk_tree_view_append_column(view, col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,"text", COLUMN_URI);

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
    msg = g_markup_printf_escaped("Field error %s.",error_string);// TODO: use _()

    /* Create the widgets */
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(buddy_list_window),
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
                        GTK_WINDOW(buddy_list_window),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_STOCK_CANCEL,
                        GTK_RESPONSE_CANCEL,
                        GTK_STOCK_APPLY,
                        GTK_RESPONSE_APPLY,
                        NULL);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid, TRUE, TRUE, 0);

    gint row = 0;
    GtkWidget *label;

    // TODO: this is ugly but temporary
    guint group_index = 0;
    guint group_count = 0;
    GtkWidget * combo_group;
    if(g_strcmp0(title, "Add new buddy") != 0)
    {
        label = gtk_label_new_with_mnemonic("_Group");
        gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
        gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
        combo_group = gtk_combo_box_text_new();
        // fill combox with existing groups
        gchar *group;
        for (guint i = 1; i < presence_group_list_get_size(); i++)
        {
            group = presence_group_list_get_nth(i);
            gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_group), group);
            group_count++;
            // set active group
            if(g_strcmp0(group, b->group) == 0)
                group_index = i - 1;
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
        if(g_strcmp0(title, "Add new buddy") != 0)
        {
            // FIXME: this function doesn't work as expected
            // if(gtk_combo_box_get_has_entry(GTK_COMBO_BOX(combo_group)))
            if(group_count > 0)
            {
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
        if((g_strcmp0(b->alias, "")==0) || (g_strcmp0(b->uri, "")==0))
        {
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
show_group_info_dialog(const gchar *title, gchar *group)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons((title),
                        GTK_WINDOW(buddy_list_window),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_STOCK_CANCEL,
                        GTK_RESPONSE_CANCEL,
                        GTK_STOCK_APPLY,
                        GTK_RESPONSE_APPLY,
                        NULL);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid, TRUE, TRUE, 0);

    gint row = 0;
    GtkWidget *label = gtk_label_new_with_mnemonic("Group");
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    GtkWidget *entry_group = gtk_entry_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry_group);
    gtk_entry_set_text(GTK_ENTRY(entry_group), group);
    gtk_grid_attach(GTK_GRID(grid), entry_group, 1, row, 1, 1);

    gtk_widget_show_all(grid);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    // update buddy OK was pressed
    if (response == GTK_RESPONSE_APPLY)
    {
        g_free(group);
        group = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry_group)));

        gtk_widget_destroy(dialog);
        return TRUE;
    }

    gtk_widget_destroy(dialog);
    return FALSE;
}


static gboolean
confirm_buddy_deletion(buddy_t *b)
{
    gchar *msg;
    account_t * acc = account_list_get_by_id(b->acc);
    msg = g_markup_printf_escaped("Are you sure want to delete \"%s\" of %s",
            b->alias, (gchar*)account_lookup(acc, CONFIG_ACCOUNT_ALIAS)); // TODO: use _()

    /* Create the widgets */
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(buddy_list_window),
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
    msg = g_markup_printf_escaped("Do you really want to delete the group %s", group);

    /* Create the widgets */
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(buddy_list_window),
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
view_popup_menu_onCallBuddy(G_GNUC_UNUSED GtkWidget *menuitem, gpointer userdata)
{
    buddy_t *b= (buddy_t*)userdata;
    callable_obj_t * c = sflphone_new_call(presence_client);

    g_free(c->_peer_number);
    c->_peer_number = g_strdup(b->uri);
    g_free(c->_accountID);
    c->_accountID = g_strdup(b->acc);

    calltree_update_call(current_calls_tab, c, presence_client, FALSE);
    sflphone_place_call(c, presence_client);
}

static void
view_popup_menu_onAddBuddy(G_GNUC_UNUSED GtkWidget *menuitem, gpointer userdata)
{
    buddy_t *b = presence_buddy_create();

    // try to grab the selected group
    if(userdata != NULL) // might be a group row or a buddy row
    {
        buddy_t *tmp_b = ((buddy_t*)userdata);
        g_free(b->group);
        b->group = g_strdup(tmp_b->group);
    }

    account_t *acc = account_list_get_current();
    g_free(b->acc);
    b->acc = g_strdup((gchar*)account_lookup(acc, CONFIG_ACCOUNT_ID));

    gchar * uri = g_strconcat("<sip:XXXX@", account_lookup(acc, CONFIG_ACCOUNT_HOSTNAME),">", NULL);
    g_free(b->uri);
    b->uri = g_strdup(uri);
    g_free(uri);

    if(show_buddy_info_dialog(_("Add new buddy"), b))
    {
        presence_buddy_list_add_buddy(b);
        update_buddylist_view();
    }
    else
        presence_buddy_delete(b);
}

static void
view_popup_menu_onRemoveBuddy (G_GNUC_UNUSED GtkWidget *menuitem, gpointer userdata)
{
    buddy_t *b = (buddy_t*) userdata;
    if(confirm_buddy_deletion(b))
    {
        presence_buddy_list_remove_buddy(b);
        update_buddylist_view();
    }
}

static void
view_popup_menu_onAddGroup (G_GNUC_UNUSED GtkWidget *menuitem, G_GNUC_UNUSED gpointer userdata)
{
    gchar *group = g_strdup("");
    if (show_group_info_dialog(_("Add group"), group))
    {
        presence_group_list_add_group(group);
        update_buddylist_view();
    }
}

static void
view_popup_menu_onRemoveGroup (G_GNUC_UNUSED GtkWidget *menuitem, gpointer userdata)
{
    gchar *group = (gchar*) userdata;
    if(confirm_group_deletion(group))
    {
        presence_group_list_remove_group(group);
        update_buddylist_view();
    }
}

static void
view_popup_menu(GdkEventButton *event, gpointer userdata, guint type)
{
    GtkWidget *menu, *menuitem;
    menu = gtk_menu_new();
    gchar* gr = g_strdup(((buddy_t*)userdata)->group); // TODO; is it necessary to be free?

    if(type == POPUP_MENU_TYPE_BUDDY)
    {
        menuitem = gtk_menu_item_new_with_label(_("New call"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
        g_signal_connect(menuitem, "activate", G_CALLBACK(view_popup_menu_onCallBuddy), userdata);

        menuitem = gtk_separator_menu_item_new();
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    }

    menuitem = gtk_menu_item_new_with_label(_("Add buddy"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    g_signal_connect(menuitem, "activate", G_CALLBACK(view_popup_menu_onAddBuddy), userdata);

    if(type == POPUP_MENU_TYPE_BUDDY)
    {
        menuitem = gtk_menu_item_new_with_label(_("Remove buddy"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
        g_signal_connect(menuitem, "activate", G_CALLBACK(view_popup_menu_onRemoveBuddy), userdata);
    }

    menuitem = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);

    menuitem = gtk_menu_item_new_with_label(_("Add group"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    g_signal_connect(menuitem, "activate", G_CALLBACK(view_popup_menu_onAddGroup), gr);

    if(type == POPUP_MENU_TYPE_GROUP)
    {
        menuitem = gtk_menu_item_new_with_label(_("Remove group"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
        g_signal_connect(menuitem, "activate", G_CALLBACK(view_popup_menu_onRemoveGroup), gr);
    }

    gtk_widget_show_all(menu);

    /* Note: event can be NULL here when called from view_onPopupMenu;
     *  gdk_event_get_time() accepts a NULL argument */
    gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
            (event != NULL) ? event->button : 0,
            gdk_event_get_time((GdkEvent*)event));
}

static buddy_t *
view_get_buddy(GtkTreeView *treeview,
        GtkTreePath *path)
{
    GtkTreeSelection * selection = gtk_tree_view_get_selection(treeview);
    buddy_t *b = NULL;
    gtk_tree_selection_unselect_all(selection);
    gtk_tree_selection_select_path(selection, path);

    GtkTreeIter   iter;
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);

    if (gtk_tree_model_get_iter(model, &iter, path))
    {
        // grab buddy data from the TreeView before displaying the menu
        GValue val;
        memset(&val, 0, sizeof(val));
        gtk_tree_model_get_value(model, &iter, COLUMN_URI, &val);
        b = presence_buddy_list_buddy_get_by_uri(g_value_dup_string(&val));
        g_value_unset(&val);
    }
    return b;
}

static gchar *
view_get_group(GtkTreeView *treeview,
        GtkTreePath *path)
{
    GtkTreeSelection * selection = gtk_tree_view_get_selection(treeview);
    gchar *group = NULL;
    gtk_tree_selection_unselect_all(selection);
    gtk_tree_selection_select_path(selection, path);

    GtkTreeIter   iter;
    GtkTreeModel *model = gtk_tree_view_get_model(treeview);

    if (gtk_tree_model_get_iter(model, &iter, path))
    {
        // grab group data from the TreeView before displaying the menu
        GValue val;
        memset(&val, 0, sizeof(val));
        gtk_tree_model_get_value(model, &iter, COLUMN_GROUP, &val);
        group = g_value_dup_string(&val);
        g_value_unset(&val);
    }
    return group;
}

static gboolean
view_onButtonPressed(GtkTreeView *treeview, GdkEventButton *event)
{
    /* single click with the right mouse button? */
    if (event->type == GDK_BUTTON_PRESS  &&  event->button == 3)
    {
        GtkTreeSelection * selection = gtk_tree_view_get_selection(treeview);

        /* Note: gtk_tree_selection_count_selected_rows() does not
         *   exist in gtk+-2.0, only in gtk+ >= v2.2 ! */
        if (gtk_tree_selection_count_selected_rows(selection)  == 1)
        {
            GtkTreePath *path;
            /* Get tree path for row that was clicked */
            if (gtk_tree_view_get_path_at_pos(treeview,
                        (gint) event->x,
                        (gint) event->y,
                        &path, NULL, NULL, NULL))
            {
                buddy_t *b = view_get_buddy(treeview, path);
                if (b != NULL)
                    view_popup_menu(event, b, POPUP_MENU_TYPE_BUDDY);
                else // a group row has been selected
                {
                    // use a fake buddy as argument
                    g_free(tmp_buddy->group);
                    tmp_buddy->group = g_strdup(view_get_group(treeview, path));
                    if (tmp_buddy->group != NULL)
                        view_popup_menu(event, tmp_buddy, POPUP_MENU_TYPE_GROUP);
                }
                gtk_tree_path_free(path);

                return TRUE;
            }
        }
        //else right click on the back ground
        // use a fake buddy as argument
        g_free(tmp_buddy->group);
        tmp_buddy->group = g_strdup(" ");
        view_popup_menu(event, tmp_buddy, POPUP_MENU_TYPE_DEFAULT);

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
        view_popup_menu_onEditBuddy(NULL, b);
}
*/

/******************************** Status bar *********************************/

/**
 * This function reads the status combo box, updates the account_schema
 * and call the the DBus presence publish method if enabled.
 * @param combo The text combo box associated with the status to be published.
 */
static void
status_changed_cb(GtkComboBox *combo)
{
    const gchar *status = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo));
    gboolean b = (g_strcmp0(status, PRESENCE_STATUS_ONLINE) == 0)? TRUE : FALSE;
    account_t * account;

    for (guint i = 0; i < account_list_get_size(); i++){
        account = account_list_get_nth(i);
        g_assert(account);
        account_replace(account, CONFIG_PRESENCE_STATUS, status);

        if((g_strcmp0(account_lookup(account, CONFIG_PRESENCE_PUBLISH_SUPPORTED), "true") == 0) &&
            (g_strcmp0(account_lookup(account, CONFIG_PRESENCE_ENABLED), "true") == 0) &&
            (((g_strcmp0(account_lookup(account, CONFIG_ACCOUNT_ENABLE), "true") == 0) ||
            (account_is_IP2IP(account)))))
        {
            dbus_presence_publish(account->accountID,b);
            g_debug("Presence : publish status of acc:%s => %s", account->accountID, status);
        }
    }
}

void
statusbar_enable_presence()
{
    account_t * account;
    gboolean global_publish_enabled = FALSE;
    gboolean global_status = FALSE;

    if(!presence_status_bar) // buddy window not opened
        return;

    /* Check if one of the registered accounts has Presence enabled */
    for (guint i = 0; i < account_list_get_size(); i++){
        account = account_list_get_nth(i);
        g_assert(account);

        if(!(account_is_IP2IP(account)) &&
                (g_strcmp0(account_lookup(account, CONFIG_PRESENCE_ENABLED), "true") == 0)/* &&
                (account->state == ACCOUNT_STATE_REGISTERED) */)
        {
            if(g_strcmp0(account_lookup(account, CONFIG_PRESENCE_STATUS), "true") == 0)
            {
                global_status = TRUE;
                g_debug("Presence : status bar, %s with status is online.", account->accountID);
            }

            if(g_strcmp0(account_lookup(account, CONFIG_PRESENCE_PUBLISH_SUPPORTED), "true") == 0)
            {
                global_publish_enabled = TRUE;
                g_debug("Presence : status bar, %s with publish enabled.", account->accountID);
            }
        }
    }

    gtk_combo_box_set_active(GTK_COMBO_BOX(presence_status_combo),  global_status? 1:0);
    gtk_widget_set_sensitive(presence_status_combo, global_publish_enabled? TRUE:FALSE);
}

void
show_all_toggled_cb()
{
    update_buddylist_view();
}

GtkWidget*
create_presence_status_bar()
{
    GtkWidget *bar = gtk_statusbar_new();

    show_all_check_box = gtk_check_button_new_with_mnemonic(_("_Show all"));
    gtk_box_pack_start(GTK_BOX(bar), show_all_check_box, TRUE, TRUE, 0);
    g_signal_connect (show_all_check_box, "toggled", G_CALLBACK(show_all_toggled_cb), NULL);

    GtkWidget *label = gtk_label_new_with_mnemonic(_("Status:"));
    gtk_box_pack_start(GTK_BOX(bar), label, TRUE, TRUE, 0);

    /* Add presence status combo_box*/
    presence_status_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(presence_status_combo), _(PRESENCE_STATUS_OFFLINE));
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(presence_status_combo), _(PRESENCE_STATUS_ONLINE));
    gtk_widget_set_sensitive(presence_status_combo, FALSE);
    gtk_box_pack_start(GTK_BOX(bar), presence_status_combo, TRUE, TRUE, 0);

    g_signal_connect(G_OBJECT(presence_status_combo), "changed", G_CALLBACK(status_changed_cb), NULL);
    statusbar_enable_presence();

    return bar;
}

/******************************** window  *********************************/
gboolean
timer_cb()
{
    presence_buddy_list_init(presence_client);
    return TRUE; // this is necessary to keep the timer alive
}

void
destroy_buddylist_window()
{
    g_debug("Destroy buddylist window ");
    buddy_list_tree_view = NULL;
    presence_status_bar = NULL;
    gtk_widget_destroy(buddy_list_window);

    gtk_toggle_action_set_active(toggle_action, FALSE);
    //presence_buddy_list_flush();
}

void
create_buddylist_window(SFLPhoneClient *client, GtkToggleAction *action)
{
    // keep track of widget which opened that window
    toggle_action = action;

    // keep track of the client
    presence_client = client;

    const gchar * title = _("SFLphone Buddies");
    g_debug("Create window : %s", title);

    buddy_list_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_container_set_border_width(GTK_CONTAINER(buddy_list_window), 0);
    gtk_window_set_title(GTK_WINDOW(buddy_list_window), title);
    gtk_window_set_default_size(GTK_WINDOW(buddy_list_window), BUDDYLIST_WINDOW_WIDTH, BUDDYLIST_WINDOW_HEIGHT);
    gtk_widget_set_name(buddy_list_window, title);

    /* Instantiate vbox, subvbox as homogeneous */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);
    gtk_container_add(GTK_CONTAINER(buddy_list_window), vbox);

    /* Create the tree view*/
    buddy_list_tree_view = create_view();
    gtk_tree_view_set_reorderable(buddy_list_tree_view, TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), GTK_WIDGET(buddy_list_tree_view), TRUE, TRUE, 5);

    /* Status bar, cntains presence_status selector */
    presence_status_bar = create_presence_status_bar();
    gtk_box_pack_start(GTK_BOX(vbox), presence_status_bar, FALSE, TRUE, 0);

    g_signal_connect(G_OBJECT(buddy_list_tree_view), "button-press-event", G_CALLBACK(view_onButtonPressed), NULL);
    //g_signal_connect(G_OBJECT(buddy_list_tree_view), "row-activated", G_CALLBACK(view_row_activated_cb), NULL);
    g_signal_connect(G_OBJECT(buddy_list_window), "button-press-event", G_CALLBACK(view_onButtonPressed), NULL);
    g_signal_connect_after(buddy_list_window, "destroy", G_CALLBACK(destroy_buddylist_window), NULL);

    /*  Load buddylist  */
    presence_buddy_list_init(presence_client);
    g_object_set_data(G_OBJECT(buddy_list_window), "Buddy-List", (gpointer)presence_buddy_list_get());
    update_buddylist_view();

    /*  timer to refresh the subscription */
    g_timeout_add_seconds(60, (GSourceFunc)timer_cb, NULL);

    tmp_buddy = presence_buddy_create();

    /* make sure that everything, window and label, are visible */
    gtk_widget_show_all(buddy_list_window);
}
