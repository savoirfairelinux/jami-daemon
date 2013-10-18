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
#include "presence.h"
#include "dbus.h"
#include "str_utils.h"

static GtkWidget *buddylistwindow;
static GtkWidget *buddy_list_tree_view = NULL;
static GtkToggleAction *toggle_action = NULL;
static buddy_t tmp_buddy;

static GtkTreeModel *create_and_fill_model (void);
static GtkWidget *create_view (void);
gboolean selection_changed(GtkTreeSelection *selection);

#define PRESENCE_DEBUG

enum
{
    COLUMN_ALIAS,
    COLUMN_STATUS,
    COLUMN_NOTE,
#ifdef PRESENCE_DEBUG
    COLUMN_URI,
    COLUMN_ACCOUNTID,
    COLUMN_SUBSCRIBED
#endif
};


#define N_COLUMN 6


static GtkTreeModel *
create_and_fill_model (void)
{
    GtkTreeStore *treestore;
    GtkTreeIter toplevel, child;
    treestore = gtk_tree_store_new(N_COLUMN, G_TYPE_STRING, // Alias
                                             G_TYPE_STRING, // Status
                                             G_TYPE_STRING, // Note
                                             G_TYPE_STRING, // URI
                                             G_TYPE_STRING, // AccID
                                             G_TYPE_STRING);// subscribed

    GList * buddy_list = g_object_get_data(G_OBJECT(buddylistwindow), "Buddy-List");
    buddy_t * buddy;

    for (guint i = 0; i < account_list_get_size(); i++)
    {
        account_t * acc = account_list_get_nth(i);
        if(!(account_is_IP2IP(acc)) && (acc->state == ACCOUNT_STATE_REGISTERED))
        {
            gchar *accID = account_lookup(acc, CONFIG_ACCOUNT_ID);
            gtk_tree_store_append(treestore, &toplevel, NULL);
            gtk_tree_store_set(treestore, &toplevel,
                    COLUMN_ALIAS, account_lookup(acc, CONFIG_ACCOUNT_ALIAS),
                    COLUMN_STATUS, account_state_name(acc->state),
                    COLUMN_NOTE, "",
                    COLUMN_URI, "",
                    COLUMN_ACCOUNTID, accID,
                    COLUMN_SUBSCRIBED, "",
                    -1);
            for (guint j =  1; j < presence_list_get_size(buddy_list); j++)
            {
                buddy = presence_list_get_nth(j);
                if(g_strcmp0(buddy->acc, accID)==0)
                {
                    gtk_tree_store_append(treestore, &child, &toplevel);
                    gtk_tree_store_set(treestore, &child,
                            COLUMN_ALIAS, buddy->alias,
                            COLUMN_STATUS, (buddy->status)? PRESENCE_STATUS_ONLINE:PRESENCE_STATUS_OFFLINE,
                            COLUMN_NOTE,  buddy->note,
                            COLUMN_URI,  buddy->uri,
                            COLUMN_ACCOUNTID, buddy->acc,
                            COLUMN_SUBSCRIBED, (buddy->subscribed)? "Active":"Inactive",
                            -1);
                }
            }
        }
    }
    return GTK_TREE_MODEL(treestore);
}

static GtkWidget *
create_view (void)
{
    GtkTreeViewColumn *col;
    GtkCellRenderer *renderer;
    GtkWidget *view;

    view = gtk_tree_view_new();

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, _("Buddies"));
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,"text", COLUMN_ALIAS);


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

#ifdef PRESENCE_DEBUG
    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "URI");
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,"text", COLUMN_URI);

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "Suscribed");
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,"text", COLUMN_SUBSCRIBED);

    col = gtk_tree_view_column_new();
    gtk_tree_view_column_set_title(col, "AccID");
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), col);
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(col, renderer, TRUE);
    gtk_tree_view_column_add_attribute(col, renderer,"text", COLUMN_ACCOUNTID);
#endif

    return view;
}


void
update_buddylist_view()
{
    if(!buddy_list_tree_view)
        return; // Buddylist window not opend

    GtkTreeModel * model = create_and_fill_model();
    gtk_tree_view_set_model(GTK_TREE_VIEW(buddy_list_tree_view), model);
    g_object_unref(model);
    gtk_tree_view_expand_all(GTK_TREE_VIEW(buddy_list_tree_view));
    g_debug("BuddyListTreeView updated.");
}


static gboolean
show_buddy_info(const gchar *title, buddy_t *b)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons((title),
                        GTK_WINDOW(buddylistwindow),
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

    GtkWidget *label = gtk_label_new_with_mnemonic("_Account");
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    GtkWidget *combo_account = gtk_combo_box_text_new();
    // fill combox with existing accounts
    guint acc_index = 0;
    account_t *acc;
    for (guint i = 0; i < account_list_get_size(); i++)
    {
        acc = account_list_get_nth(i);
        gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(combo_account),
                account_lookup(acc, CONFIG_ACCOUNT_ALIAS));
        // set active acc
        if(g_strcmp0(account_lookup(acc, CONFIG_ACCOUNT_ID), b->acc) == 0)
            acc_index = i;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(combo_account), (gint)acc_index);

    gtk_grid_attach(GTK_GRID(grid), combo_account, 1, row, 1, 1);

    row++;
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
        gchar * alias = gtk_combo_box_text_get_active_text(GTK_COMBO_BOX_TEXT(combo_account));
        acc = account_list_get_by_alias(alias);
        g_free(alias);

        g_free(b->acc);
        g_free(b->alias);
        g_free(b->uri);
        b->acc = g_strdup(acc->accountID);
        b->alias = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry_alias)));
        b->uri = g_strdup(gtk_entry_get_text(GTK_ENTRY(entry_uri)));

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
    GtkWidget *dialog = gtk_message_dialog_new(GTK_WINDOW(buddylistwindow),
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

static void
view_popup_menu_onEdit (G_GNUC_UNUSED GtkWidget *menuitem, gpointer userdata)
{
    buddy_t *backup = g_malloc(sizeof(buddy_t));
    memcpy(backup, (buddy_t*)userdata, sizeof(buddy_t));

    if(show_buddy_info(_("Edit buddy"), (buddy_t *)userdata))
    {
        presence_list_update_buddy((buddy_t *)userdata, backup);
        update_buddylist_view();
    }
    g_free(backup);
}

static void
view_popup_menu_onAdd (G_GNUC_UNUSED GtkWidget *menuitem, gpointer userdata)
{
    buddy_t *b = presence_buddy_create();
    g_free(b->acc);
    b->acc = g_strdup(((buddy_t*)userdata)->acc);

    account_t * acc = account_list_get_by_id(b->acc);
    gchar * uri = g_strconcat("<sip:XXXX@", account_lookup(acc, CONFIG_ACCOUNT_HOSTNAME),">", NULL);
    g_free(b->uri);
    b->uri = g_strdup(uri);
    g_free(uri);

    g_free(b->note);
    b->note = g_strdup("Not Found.");

    if(show_buddy_info(_("Add new buddy"), b))
    {
        presence_list_add_buddy(b);
        update_buddylist_view();
    }
    else
        presence_buddy_delete(b);
}

static void
view_popup_menu_onRemove (G_GNUC_UNUSED GtkWidget *menuitem, gpointer userdata)
{
    buddy_t *b = (buddy_t*) userdata;
    const gboolean confirmed = confirm_buddy_deletion(b);
    if(confirmed)
    {
        presence_list_remove_buddy(b);
        update_buddylist_view();
    }
}

static void
view_popup_menu (G_GNUC_UNUSED GtkWidget *treeview, GdkEventButton *event, gpointer userdata)
{
g_print("0\n");
    GtkWidget *menu, *menuitem;
    menu = gtk_menu_new();
    buddy_t *b = (buddy_t*) userdata;
g_print("1\n");
    menuitem = gtk_menu_item_new_with_label(_("Add"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
    g_signal_connect(menuitem, "activate",
            G_CALLBACK(view_popup_menu_onAdd), userdata);

    if(strlen(b->uri)>0) // if a buddy was selected, not an account
    {
        menuitem = gtk_menu_item_new_with_label(_("Edit"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
        g_signal_connect(menuitem, "activate",
                G_CALLBACK(view_popup_menu_onEdit), userdata);

        menuitem = gtk_menu_item_new_with_label(_("Remove"));
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), menuitem);
        g_signal_connect(menuitem, "activate",
                G_CALLBACK(view_popup_menu_onRemove), userdata);
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
    GtkTreeSelection * selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
    buddy_t *b = NULL;
    gtk_tree_selection_unselect_all(selection);
    gtk_tree_selection_select_path(selection, path);

    GtkTreeIter   iter;
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));

    if (gtk_tree_model_get_iter(model, &iter, path))
    {
        // grab buddy data from the TreeView before displaying the menu
        buddy_t *tmp = &tmp_buddy;
        GValue val;

        memset(&val, 0, sizeof(val));
        gtk_tree_model_get_value(model, &iter, COLUMN_ACCOUNTID, &val);
        g_free(tmp->acc);
        tmp->acc = g_value_dup_string(&val);
        gtk_tree_model_get_value(model, &iter, COLUMN_URI, &val);
        g_free(tmp->uri);
        tmp->uri = g_value_dup_string(&val);
        gtk_tree_model_get_value(model, &iter, COLUMN_ALIAS, &val);
        g_free(tmp->alias);
        tmp->alias = g_value_dup_string(&val);
        g_value_unset(&val);

        b = presence_list_buddy_get_by_string(tmp->acc, tmp->uri);
    }
    return b;
}

static gboolean
view_onButtonPressed (GtkWidget *treeview, GdkEventButton *event)
{
    /* single click with the right mouse button? */
    if (event->type == GDK_BUTTON_PRESS  &&  event->button == 3)
    {
        GtkTreeSelection * selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(treeview));
        //buddy_t *b;

        /* Note: gtk_tree_selection_count_selected_rows() does not
         *   exist in gtk+-2.0, only in gtk+ >= v2.2 ! */
        if (gtk_tree_selection_count_selected_rows(selection)  <= 1)
        {
            GtkTreePath *path;
            /* Get tree path for row that was clicked */
            if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(treeview),
                        (gint) event->x,
                        (gint) event->y,
                        &path, NULL, NULL, NULL))
            {
                buddy_t *b = view_get_buddy(GTK_TREE_VIEW(treeview), path);

                /* b might be NULL. This means an account was selected instead of a buddy
                 * Only the Add function will available and the dialog window will
                 * only display the account alias that must be grabbed.*/
                if(!b)
                    b = &tmp_buddy; // at this point tmp_buddy contains the account ID

                gtk_tree_path_free(path);
                view_popup_menu(treeview, event, b);
            }
        }
        return TRUE;
    }
    return FALSE;
}

static gboolean
view_onPopupMenu (GtkWidget *treeview, gpointer userdata)
{
    view_popup_menu(treeview, NULL, userdata);

    return TRUE; /* we handled this */
}

static void
view_row_activated_cb(GtkTreeView *treeview,
        GtkTreePath *path,
        G_GNUC_UNUSED GtkTreeViewColumn *col)
{

    buddy_t *b = view_get_buddy(treeview, path);
    if(b) //"NULL if an account was selected instead of a buddy.
        view_popup_menu_onEdit(NULL, b);
}

void
destroy_buddylist_window()
{
    g_debug("Destroy buddylist window ");
    buddy_list_tree_view = NULL;
    gtk_widget_destroy(buddylistwindow);

    gtk_toggle_action_set_active(toggle_action, FALSE);
    presence_list_flush();
}

void
create_buddylist_window(SFLPhoneClient *client, GtkToggleAction *action)
{
    // keep track of widget which opened that window
    toggle_action = action;

    const gchar * title = _("SFLphone Buddies");
    g_debug("Create window : %s", title);

    buddylistwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    gtk_container_set_border_width(GTK_CONTAINER(buddylistwindow), 0);
    gtk_window_set_title(GTK_WINDOW(buddylistwindow), title);
    gtk_window_set_default_size(GTK_WINDOW(buddylistwindow), BUDDYLIST_WINDOW_WIDTH, BUDDYLIST_WINDOW_HEIGHT);
    gtk_widget_set_name(buddylistwindow, title);

    /* Instantiate vbox, subvbox as homogeneous */
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_set_homogeneous(GTK_BOX(vbox), FALSE);
    gtk_container_add(GTK_CONTAINER(buddylistwindow), vbox);

    /* Create the tree view*/
    buddy_list_tree_view = create_view();
    gtk_box_pack_start(GTK_BOX(vbox), buddy_list_tree_view, TRUE, TRUE, 5);

    g_signal_connect(G_OBJECT(buddy_list_tree_view), "button-press-event", G_CALLBACK(view_onButtonPressed), NULL);
    g_signal_connect(G_OBJECT(buddy_list_tree_view), "popup-menu", G_CALLBACK(view_onPopupMenu), NULL);
    g_signal_connect(G_OBJECT(buddy_list_tree_view), "row-activated", G_CALLBACK(view_row_activated_cb), NULL);
    g_signal_connect_after(buddylistwindow, "destroy", (GCallback)destroy_buddylist_window, NULL);

    // Load buddylist
    presence_list_init(client);
    g_object_set_data(G_OBJECT(buddylistwindow), "Buddy-List", (gpointer)presence_list_get());
    update_buddylist_view();

    /* make sure that everything, window and label, are visible */
    gtk_widget_show_all(buddylistwindow);
}
