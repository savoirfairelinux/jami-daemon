/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
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

#include "accountlistconfigdialog.h"
#include "str_utils.h"
#include "dbus/dbus.h"
#include "accountconfigdialog.h"
#include "accountlist.h"
#include "actions.h"
#include "mainwindow.h"
#include "utils.h"
#include "unused.h"
#include "logger.h"
#include <glib/gi18n.h>
#include <string.h>

static const int CONTEXT_ID_REGISTRATION = 0;

static GtkWidget *add_button;
static GtkWidget *edit_button;
static GtkWidget *delete_button;
static GtkWidget *move_down_button;
static GtkWidget *move_up_button;
static GtkWidget *status_bar;
static GtkListStore *account_store;
static GtkDialog *account_list_dialog;
static account_t *selected_account;

// Account properties
enum {
    COLUMN_ACCOUNT_ALIAS,
    COLUMN_ACCOUNT_TYPE,
    COLUMN_ACCOUNT_STATUS,
    COLUMN_ACCOUNT_ACTIVE,
    COLUMN_ACCOUNT_DATA,
    COLUMN_ACCOUNT_COUNT
};

static void delete_account_cb(void)
{
    RETURN_IF_NULL(selected_account, "No selected account in delete action");
    dbus_remove_account(selected_account->accountID);
    selected_account = NULL;
}

static void row_activated_cb(GtkTreeView *view UNUSED,
                             GtkTreePath *path UNUSED,
                             GtkTreeViewColumn *col UNUSED,
                             gpointer user_data UNUSED)
{
    RETURN_IF_NULL(selected_account, "No selected account in edit action");
    DEBUG("%s: accountID=%s\n", __PRETTY_FUNCTION__, selected_account->accountID);
    show_account_window(selected_account);
}

static void edit_account_cb(GtkButton *button UNUSED, gpointer data UNUSED)
{
    RETURN_IF_NULL(selected_account, "No selected account in edit action");
    DEBUG("%s: accountID=%s\n", __PRETTY_FUNCTION__, selected_account->accountID);
    show_account_window(selected_account);
}

static void add_account_cb(void)
{
    account_t *new_account = create_default_account();
    show_account_window(new_account);
}

static void account_store_fill(GtkTreeIter *iter, account_t *a)
{
    const gchar *enabled = g_hash_table_lookup(a->properties, ACCOUNT_ENABLED);
    const gchar *type = g_hash_table_lookup(a->properties, ACCOUNT_TYPE);
    DEBUG("Config: Filling accounts: Account is enabled :%s", enabled);

    gtk_list_store_set(account_store, iter, COLUMN_ACCOUNT_ALIAS,
                       g_hash_table_lookup(a->properties, ACCOUNT_ALIAS),
                       COLUMN_ACCOUNT_TYPE, type,
                       COLUMN_ACCOUNT_STATUS, account_state_name(a->state),
                       COLUMN_ACCOUNT_ACTIVE, utf8_case_equal(enabled, "true"),
                       COLUMN_ACCOUNT_DATA, a, -1);
}

/**
 * Fills the treelist with accounts
 */
void account_list_config_dialog_fill()
{
    RETURN_IF_NULL(account_list_dialog, "No account dialog");

    gtk_list_store_clear(account_store);

    // IP2IP account must be first
    account_t *ip2ip = account_list_get_by_id("IP2IP");

    RETURN_IF_NULL(ip2ip, "Could not find IP2IP account");

    GtkTreeIter iter;
    gtk_list_store_append(account_store, &iter);

    account_store_fill(&iter, ip2ip);

    for (size_t i = 0; i < account_list_get_size(); ++i) {
        account_t *a = account_list_get_nth(i);
        RETURN_IF_NULL(a, "Account %d is NULL", i);

        // we don't want to process the IP2IP twice
        if (a != ip2ip) {
            gtk_list_store_append(account_store, &iter);
            account_store_fill(&iter, a);
        }
    }
}

/**
 * Call back when the user click on an account in the list
 */
static void
select_account_cb(GtkTreeSelection *selection, GtkTreeModel *model)
{
    GtkTreeIter iter;
    if (!gtk_tree_selection_get_selected(selection, &model, &iter)) {
        selected_account = NULL;
        gtk_widget_set_sensitive(move_up_button, FALSE);
        gtk_widget_set_sensitive(move_down_button, FALSE);
        gtk_widget_set_sensitive(edit_button, FALSE);
        gtk_widget_set_sensitive(delete_button, FALSE);
        return;
    }

    // The Gvalue will be initialized in the following function
    GValue val;
    memset(&val, 0, sizeof(val));
    gtk_tree_model_get_value(model, &iter, COLUMN_ACCOUNT_DATA, &val);

    selected_account = (account_t*) g_value_get_pointer(&val);
    g_value_unset(&val);

    RETURN_IF_NULL(selected_account, "Selected account is NULL");
    DEBUG("Selected account has accountID %s", selected_account->accountID);

    gtk_widget_set_sensitive(edit_button, TRUE);

    if (!account_is_IP2IP(selected_account)) {
        gtk_widget_set_sensitive(move_up_button, TRUE);
        gtk_widget_set_sensitive(move_down_button, TRUE);
        gtk_widget_set_sensitive(delete_button, TRUE);

        /* Update status bar about current registration state */
        gtk_statusbar_pop(GTK_STATUSBAR(status_bar), CONTEXT_ID_REGISTRATION);

        const gchar *state_name = account_state_name(selected_account->state);
        if (selected_account->protocol_state_description != NULL
                && selected_account->protocol_state_code != 0) {

            gchar * response = g_strdup_printf(
                                   _("Server returned \"%s\" (%d)"),
                                   selected_account->protocol_state_description,
                                   selected_account->protocol_state_code);
            gchar * message = g_strconcat(state_name, ". ", response, NULL);
            gtk_statusbar_push(GTK_STATUSBAR(status_bar),
                               CONTEXT_ID_REGISTRATION, message);

            g_free(response);
            g_free(message);

        } else {
            gtk_statusbar_push(GTK_STATUSBAR(status_bar),
                               CONTEXT_ID_REGISTRATION, state_name);
        }
    } else {
        gtk_widget_set_sensitive(move_up_button, FALSE);
        gtk_widget_set_sensitive(move_down_button, FALSE);
        gtk_widget_set_sensitive(delete_button, FALSE);
    }
    DEBUG("Selecting account in account window");
}

static void
enable_account_cb(GtkCellRendererToggle *rend UNUSED, gchar* path,
                  gpointer data)
{
    // The IP2IP profile can't be disabled
    if (utf8_case_equal(path, "0"))
        return;

    // Get pointer on object
    GtkTreePath *tree_path = gtk_tree_path_new_from_string(path);
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(data));
    GtkTreeIter iter;
    gtk_tree_model_get_iter(model, &iter, tree_path);
    gboolean enable;
    account_t* acc;
    gtk_tree_model_get(model, &iter, COLUMN_ACCOUNT_ACTIVE, &enable,
                       COLUMN_ACCOUNT_DATA, &acc, -1);
    enable = !enable;

    DEBUG("Account is %d enabled", enable);
    // Store value
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_ACCOUNT_ACTIVE,
                       enable, -1);

    // Modify account state
    gchar * registration_state = enable ? g_strdup("true") : g_strdup("false");

    DEBUG("Replacing registration state with %s", registration_state);
    g_hash_table_replace(acc->properties, g_strdup(ACCOUNT_ENABLED),
                         registration_state);

    dbus_send_register(acc->accountID, enable);
}

/**
 * Move account in list depending on direction and selected account
 */
static void
account_move(gboolean move_up, gpointer data)
{
    // Get view, model and selection of codec store
    GtkTreeView *tree_view = GTK_TREE_VIEW(data);
    GtkTreeModel *model = gtk_tree_view_get_model(tree_view);
    GtkTreeSelection *selection = gtk_tree_view_get_selection(tree_view);

    // Find selected iteration and create a copy
    GtkTreeIter iter;
    gtk_tree_selection_get_selected(selection, &model, &iter);
    GtkTreeIter *iter_copy;
    iter_copy = gtk_tree_iter_copy(&iter);

    // Find path of iteration
    gchar *path = gtk_tree_model_get_string_from_iter(model, &iter);

    // The first real account in the list can't move up because of the IP2IP account
    // It can still move down though
    if (utf8_case_equal(path, "1") && move_up)
        return;

    GtkTreePath *tree_path = gtk_tree_path_new_from_string(path);
    gint *indices = gtk_tree_path_get_indices(tree_path);
    const gint pos = indices[0];

    // Depending on button direction get new path
    if (move_up)
        gtk_tree_path_prev(tree_path);
    else
        gtk_tree_path_next(tree_path);

    gtk_tree_model_get_iter(model, &iter, tree_path);

    // Swap iterations if valid
    if (gtk_list_store_iter_is_valid(GTK_LIST_STORE(model), &iter))
        gtk_list_store_swap(GTK_LIST_STORE(model), &iter, iter_copy);

    // Scroll to new position
    gtk_tree_view_scroll_to_cell(tree_view, tree_path, NULL, FALSE, 0, 0);

    // Free resources
    gtk_tree_path_free(tree_path);
    gtk_tree_iter_free(iter_copy);
    g_free(path);

    // Perpetuate changes in account queue
    if (move_up)
        account_list_move_up(pos);
    else
        account_list_move_down(pos);

    // Set the order in the configuration file
    gchar *ordered_account_list = account_list_get_ordered_list();
    dbus_set_accounts_order(ordered_account_list);
    g_free(ordered_account_list);
}

/**
 * Called from move up account button signal
 */
static void
move_up_cb(GtkButton *button UNUSED, gpointer data)
{
    // Change tree view ordering and get index changed
    account_move(TRUE, data);
}

/**
 * Called from move down account button signal
 */
static void
move_down_cb(GtkButton *button UNUSED, gpointer data)
{
    // Change tree view ordering and get index changed
    account_move(FALSE, data);
}

static void
help_contents_cb(GtkWidget * widget UNUSED,
                 gpointer data UNUSED)
{
    GError *error = NULL;
    gtk_show_uri(NULL, "ghelp:sflphone?accounts", GDK_CURRENT_TIME, &error);
    if (error != NULL) {
        g_warning("%s", error->message);
        g_error_free(error);
    }
}

static void
close_dialog_cb(GtkWidget * widget UNUSED, gpointer data UNUSED)
{
    gtk_dialog_response(GTK_DIALOG(account_list_dialog), GTK_RESPONSE_ACCEPT);
}

static void
highlight_ip_profile(GtkTreeViewColumn *col UNUSED, GtkCellRenderer *rend,
                     GtkTreeModel *tree_model, GtkTreeIter *iter,
                     gpointer data UNUSED)
{
    GValue val;
    memset(&val, 0, sizeof(val));
    gtk_tree_model_get_value(tree_model, iter, COLUMN_ACCOUNT_DATA, &val);
    account_t *current = (account_t*) g_value_get_pointer(&val);

    g_value_unset(&val);

    if (current != NULL) {
        // Make the first line appear differently
        if (account_is_IP2IP(current)) {
            g_object_set(G_OBJECT(rend), "weight", PANGO_WEIGHT_THIN, "style",
                         PANGO_STYLE_ITALIC, "stretch",
                         PANGO_STRETCH_ULTRA_EXPANDED, "scale", 0.95, NULL);
        } else {
            g_object_set(G_OBJECT(rend), "weight", PANGO_WEIGHT_MEDIUM,
                         "style", PANGO_STYLE_NORMAL, "stretch",
                         PANGO_STRETCH_NORMAL, "scale", 1.0, NULL);
        }
    }
}

static const gchar*
state_color(account_t *a)
{
    if (!account_is_IP2IP(a))
        if (a->state == ACCOUNT_STATE_REGISTERED)
            return "Dark Green";
        else
            return "Dark Red";
    else
        return "Black";
}

static void
highlight_registration(GtkTreeViewColumn *col UNUSED, GtkCellRenderer *rend,
                       GtkTreeModel *tree_model, GtkTreeIter *iter,
                       gpointer data UNUSED)
{
    GValue val;
    memset(&val, 0, sizeof(val));
    gtk_tree_model_get_value(tree_model, iter, COLUMN_ACCOUNT_DATA, &val);
    account_t *current = (account_t*) g_value_get_pointer(&val);
    g_value_unset(&val);

    if (current)
        g_object_set(G_OBJECT(rend), "foreground", state_color(current), NULL);
}

/**
 * Account settings tab
 */
static GtkWidget*
create_account_list()
{
    selected_account = NULL;
    GtkWidget *table = gtk_table_new(1, 2, FALSE /* homogeneous */);
    gtk_table_set_col_spacings(GTK_TABLE(table), 10);
    gtk_container_set_border_width(GTK_CONTAINER(table), 10);

    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window),
                                   GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window),
                                        GTK_SHADOW_IN);
    gtk_table_attach(GTK_TABLE(table), scrolled_window, 0, 1, 0, 1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    account_store = gtk_list_store_new(COLUMN_ACCOUNT_COUNT,
                                       G_TYPE_STRING,  // Name
                                       G_TYPE_STRING,  // Protocol
                                       G_TYPE_STRING,  // Status
                                       G_TYPE_BOOLEAN, // Enabled / Disabled
                                       G_TYPE_POINTER  // Pointer to the Object
                                      );

    account_list_config_dialog_fill();

    GtkTreeView * tree_view = GTK_TREE_VIEW(gtk_tree_view_new_with_model(GTK_TREE_MODEL(account_store)));
    GtkTreeSelection *tree_selection = gtk_tree_view_get_selection(tree_view);
    g_signal_connect(G_OBJECT(tree_selection), "changed",
                     G_CALLBACK(select_account_cb),
                     account_store);

    GtkCellRenderer *renderer = gtk_cell_renderer_toggle_new();
    GtkTreeViewColumn *tree_view_column =
        gtk_tree_view_column_new_with_attributes("Enabled", renderer, "active",
                                                 COLUMN_ACCOUNT_ACTIVE , NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), tree_view_column);
    g_signal_connect(G_OBJECT(renderer), "toggled", G_CALLBACK(enable_account_cb), (gpointer) tree_view);

    renderer = gtk_cell_renderer_text_new();
    tree_view_column = gtk_tree_view_column_new_with_attributes("Alias",
                                                                renderer,
                                                                "markup",
                                                                COLUMN_ACCOUNT_ALIAS,
                                                                NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), tree_view_column);

    // A double click on the account line opens the window to edit the account
    g_signal_connect(G_OBJECT(tree_view), "row-activated", G_CALLBACK(row_activated_cb), NULL);
    gtk_tree_view_column_set_cell_data_func(tree_view_column, renderer,
                                            highlight_ip_profile, NULL, NULL);

    renderer = gtk_cell_renderer_text_new();
    tree_view_column = gtk_tree_view_column_new_with_attributes(_("Protocol"),
                                                                renderer,
                                                                "markup",
                                                                COLUMN_ACCOUNT_TYPE,
                                                                NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), tree_view_column);
    gtk_tree_view_column_set_cell_data_func(tree_view_column, renderer,
                                            highlight_ip_profile, NULL, NULL);

    renderer = gtk_cell_renderer_text_new();
    tree_view_column = gtk_tree_view_column_new_with_attributes(_("Status"),
                     renderer,
                     "markup", COLUMN_ACCOUNT_STATUS,
                     NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), tree_view_column);
    // Highlight IP profile
    gtk_tree_view_column_set_cell_data_func(tree_view_column, renderer,
                                            highlight_ip_profile, NULL, NULL);
    // Highlight account registration state
    gtk_tree_view_column_set_cell_data_func(tree_view_column, renderer,
                                            highlight_registration, NULL,
                                            NULL);

    g_object_unref(G_OBJECT(account_store));

    gtk_container_add(GTK_CONTAINER(scrolled_window), GTK_WIDGET(tree_view));

    /* The buttons to press! */
    GtkWidget *button_box = gtk_button_box_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_set_spacing(GTK_BOX(button_box), 10);
    gtk_button_box_set_layout(GTK_BUTTON_BOX(button_box), GTK_BUTTONBOX_START);
    gtk_table_attach(GTK_TABLE(table), button_box, 1, 2, 0, 1,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    move_up_button = gtk_button_new_from_stock(GTK_STOCK_GO_UP);
    gtk_widget_set_sensitive(move_up_button, FALSE);
    gtk_box_pack_start(GTK_BOX(button_box), move_up_button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(move_up_button), "clicked",
                     G_CALLBACK(move_up_cb), tree_view);

    move_down_button = gtk_button_new_from_stock(GTK_STOCK_GO_DOWN);
    gtk_widget_set_sensitive(move_down_button, FALSE);
    gtk_box_pack_start(GTK_BOX(button_box), move_down_button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(move_down_button), "clicked",
                     G_CALLBACK(move_down_cb), tree_view);

    add_button = gtk_button_new_from_stock(GTK_STOCK_ADD);
    g_signal_connect_swapped(G_OBJECT(add_button), "clicked",
                             G_CALLBACK(add_account_cb), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), add_button, FALSE, FALSE, 0);

    edit_button = gtk_button_new_from_stock(GTK_STOCK_EDIT);
    gtk_widget_set_sensitive(edit_button, FALSE);
    g_signal_connect(G_OBJECT(edit_button), "clicked", G_CALLBACK(edit_account_cb), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), edit_button, FALSE, FALSE, 0);

    delete_button = gtk_button_new_from_stock(GTK_STOCK_REMOVE);
    gtk_widget_set_sensitive(delete_button, FALSE);
    g_signal_connect_swapped(G_OBJECT(delete_button), "clicked",
                             G_CALLBACK(delete_account_cb), NULL);
    gtk_box_pack_start(GTK_BOX(button_box), delete_button, FALSE, FALSE, 0);

    /* help and close buttons */
    GtkWidget * buttonHbox = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_table_attach(GTK_TABLE(table), buttonHbox, 0, 2, 1, 2,
                     GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 10);

    GtkWidget * helpButton = gtk_button_new_from_stock(GTK_STOCK_HELP);
    g_signal_connect_swapped(G_OBJECT(helpButton), "clicked",
                             G_CALLBACK(help_contents_cb), NULL);
    gtk_box_pack_start(GTK_BOX(buttonHbox), helpButton, FALSE, FALSE, 0);

    GtkWidget * closeButton = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
    g_signal_connect_swapped(G_OBJECT(closeButton), "clicked",
                             G_CALLBACK(close_dialog_cb), NULL);
    gtk_box_pack_start(GTK_BOX(buttonHbox), closeButton, FALSE, FALSE, 0);

    gtk_widget_show_all(table);

    /* Resize the scrolled window for a better view */
    GtkRequisition requisition;
    gtk_widget_get_preferred_size(GTK_WIDGET(tree_view), NULL, &requisition);
    gtk_widget_set_size_request(scrolled_window, requisition.width + 20,
                                requisition.height);
    GtkRequisition requisitionButton;
    gtk_widget_get_preferred_size(delete_button, NULL, &requisitionButton);
    gtk_widget_set_size_request(closeButton, requisitionButton.width, -1);
    gtk_widget_set_size_request(helpButton, requisitionButton.width, -1);

    gtk_widget_show_all(table);

    return table;
}

void show_account_list_config_dialog(void)
{
    account_list_dialog = GTK_DIALOG(gtk_dialog_new_with_buttons(_("Accounts"),
                                     GTK_WINDOW(get_main_window()),
                                     GTK_DIALOG_DESTROY_WITH_PARENT, NULL,
                                     NULL));

    /* Set window properties */
    gtk_container_set_border_width(GTK_CONTAINER(account_list_dialog), 0);
    gtk_window_set_resizable(GTK_WINDOW(account_list_dialog), FALSE);

    GtkWidget *accountFrame = gnome_main_section_new(_("Configured Accounts"));
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(account_list_dialog)),
                       accountFrame, TRUE, TRUE, 0);
    gtk_widget_show(accountFrame);

    /* Accounts tab */
    GtkWidget *tab = create_account_list();
    gtk_widget_show(tab);
    gtk_container_add(GTK_CONTAINER(accountFrame), tab);

    /* Status bar for the account list */
    status_bar = gtk_statusbar_new();
    gtk_widget_show(status_bar);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(account_list_dialog)), status_bar, TRUE, TRUE, 0);

    const gint num_accounts = account_list_get_registered_accounts();

    if (num_accounts) {
        gchar * message = g_strdup_printf(n_("There is %d active account",
                                             "There are %d active accounts",
                                             num_accounts), num_accounts);
        gtk_statusbar_push(GTK_STATUSBAR(status_bar), CONTEXT_ID_REGISTRATION,
                           message);
        g_free(message);
    } else {
        gtk_statusbar_push(GTK_STATUSBAR(status_bar), CONTEXT_ID_REGISTRATION,
                           _("You have no active account"));
    }

    gtk_dialog_run(account_list_dialog);

    status_bar_display_account();

    gtk_widget_destroy(GTK_WIDGET(account_list_dialog));
    account_list_dialog = NULL;
    update_actions();
}

