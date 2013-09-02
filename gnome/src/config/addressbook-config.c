/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#include "addressbook-config.h"
#include "utils.h"
#include "str_utils.h"
#include "dbus.h"
#include "searchbar.h"
#include "contacts/addrbookfactory.h"
#include "sflphone_client.h"
#include <glib/gi18n.h>
#include <string.h>
#include <stdlib.h>

static AddressBook_Config *addressbook_config;
static GtkWidget *book_tree_view;

static GtkWidget *photo;
static GtkWidget *cards_label;
static GtkWidget *scale_label;
static GtkWidget *scrolled_label;
static GtkWidget *scrolled_window;
static GtkWidget *scale_button;
static GtkWidget *business;
static GtkWidget *mobile;
static GtkWidget *home;

enum {
    COLUMN_BOOK_ACTIVE, COLUMN_BOOK_NAME, COLUMN_BOOK_UID
};

AddressBook_Config *
addressbook_config_load_parameters(GSettings *settings)
{
    static AddressBook_Config defconfig;
    defconfig.enable = g_settings_get_boolean(settings,
            "use-evolution-addressbook");
    defconfig.max_results = g_settings_get_int(settings,
            "addressbook-max-results");
    defconfig.display_contact_photo = g_settings_get_boolean(settings,
            "addressbook-display-photo");
    defconfig.search_phone_business = g_settings_get_boolean(settings,
            "addressbook-search-phone-business");
    defconfig.search_phone_home = g_settings_get_boolean(settings,
            "addressbook-search-phone-home");
    defconfig.search_phone_mobile = g_settings_get_boolean(settings,
            "addressbook-search-phone-mobile");

    return &defconfig;
}

void
addressbook_config_save_parameters(GSettings *settings)
{
    AddressBook_Config *config = addressbook_config;
    g_settings_set_boolean(settings, "use-evolution-addressbook",
            config->enable);
    g_settings_set_int(settings, "addressbook-max-results",
            config->max_results);
    g_settings_set_boolean(settings, "addressbook-display-photo",
            config->display_contact_photo);
    g_settings_set_boolean(settings, "addressbook-search-phone-business",
            config->search_phone_business);
    g_settings_set_boolean(settings, "addressbook-search-phone-home",
            config->search_phone_home);
    g_settings_set_boolean(settings, "addressbook-search-phone-mobile",
            config->search_phone_mobile);

    update_searchbar_addressbook_list(settings);
}

void
enable_options()
{

    if (!addressbook_config->enable) {
        g_debug("Disable addressbook options\n");
        gtk_widget_set_sensitive(photo, FALSE);
        gtk_widget_set_sensitive(scrolled_label, FALSE);
        gtk_widget_set_sensitive(cards_label, FALSE);
        gtk_widget_set_sensitive(scrolled_window, FALSE);
        gtk_widget_set_sensitive(scale_button, FALSE);
        gtk_widget_set_sensitive(scale_label, FALSE);
        gtk_widget_set_sensitive(business, FALSE);
        gtk_widget_set_sensitive(mobile, FALSE);
        gtk_widget_set_sensitive(home, FALSE);
        gtk_widget_set_sensitive(book_tree_view, FALSE);


    } else {
        g_debug("Enable addressbook options\n");
        gtk_widget_set_sensitive(photo, TRUE);
        gtk_widget_set_sensitive(scrolled_label, TRUE);
        gtk_widget_set_sensitive(cards_label, TRUE);
        gtk_widget_set_sensitive(scrolled_window, TRUE);
        gtk_widget_set_sensitive(scale_button, TRUE);
        gtk_widget_set_sensitive(scale_label, TRUE);
        gtk_widget_set_sensitive(business, TRUE);
        gtk_widget_set_sensitive(mobile, TRUE);
        gtk_widget_set_sensitive(home, TRUE);
        gtk_widget_set_sensitive(book_tree_view, TRUE);
    }
}

static void
enable_cb(GtkWidget *widget, SFLPhoneClient *client)
{

    addressbook_config->enable = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    g_settings_set_boolean(client->settings, "use-evolution-addressbook", addressbook_config->enable);
    enable_options();
}

static void
max_results_cb(GtkSpinButton *button)
{
    addressbook_config->max_results = gtk_spin_button_get_value_as_int(button);
}

static void
display_contact_photo_cb(GtkWidget *widget)
{

    addressbook_config->display_contact_photo
    = (guint) gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void
search_phone_business_cb(GtkWidget *widget)
{

    addressbook_config->search_phone_business
    = (guint) gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

static void
search_phone_home_cb(GtkWidget *widget)
{

    addressbook_config->search_phone_home = (guint) gtk_toggle_button_get_active(
            GTK_TOGGLE_BUTTON(widget));
}

static void
search_phone_mobile_cb(GtkWidget *widget)
{

    addressbook_config->search_phone_mobile
    = (guint) gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
}

/**
 * Toggle active value of book on click and update changes to the deamon
 * and in configuration files
 */
static void
addressbook_config_book_active_toggled(
    G_GNUC_UNUSED GtkCellRendererToggle *renderer, gchar *path, gpointer data)
{
    GtkTreeIter iter;
    GtkTreePath *treePath;
    GtkTreeModel *model;
    gboolean active;
    book_data_t *book_data;
    gchar* name;
    gchar* uid;

    if (!addrbook)
        return;

    // Get path of clicked book active toggle box
    treePath = gtk_tree_path_new_from_string(path);

    if (!(model = gtk_tree_view_get_model(GTK_TREE_VIEW(data)))) {
        g_debug("No valid model (%s:%d)", __FILE__, __LINE__);
        return;
    }

    gtk_tree_model_get_iter(model, &iter, treePath);

    // Get active value  at iteration
    gtk_tree_model_get(model, &iter, COLUMN_BOOK_ACTIVE, &active,
                       COLUMN_BOOK_UID, &uid, COLUMN_BOOK_NAME, &name, -1);

    // Toggle active value
    active = !active;

    // Store value
    gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_BOOK_ACTIVE, active, -1);

    gtk_tree_path_free(treePath);

    // Update current memory stored books data
    book_data = addrbook->get_book_data_by_uid(uid);

    if (book_data == NULL) {
        g_warning("Could not find addressbook %s", uid);
        return;
    }

    book_data->active = active;

    // Save data
    gboolean valid;

    /* Get the first iter in the list */
    valid = gtk_tree_model_get_iter_first(model, &iter);

    GPtrArray *array = g_ptr_array_new();

    while (valid) {
        // Get active value at iteration
        gtk_tree_model_get(model, &iter, COLUMN_BOOK_ACTIVE, &active,
                           COLUMN_BOOK_UID, &uid, COLUMN_BOOK_NAME, &name, -1);

        if (active)
            g_ptr_array_add(array, uid);

        valid = gtk_tree_model_iter_next(model, &iter);
    }

    // Allocate NULL array at the end for Dbus
    g_ptr_array_add(array, NULL);
    // copy to a gchar ** for Dbus
    const gchar **list = g_malloc(sizeof(gchar *) * array->len);
    for (guint i = 0; i < array->len; ++i)
        list[i] = g_ptr_array_index(array, i);
    g_ptr_array_free(array, TRUE);

    // free the list, but not its elements as they live in the tree model
    g_free(list);
}

static void
addressbook_config_fill_book_list()
{
    GtkTreeIter list_store_iterator;
    GSList *book_list_iterator;
    GtkListStore *store;
    book_data_t *book_data;

    if (!addrbook)
        return;

    GSList *books_data = addrbook->get_books_data();

    if (!books_data)
        g_debug("No valid books data");

    // Get model of view and clear it
    if (!(store = GTK_LIST_STORE(gtk_tree_view_get_model(GTK_TREE_VIEW(book_tree_view))))) {
        g_debug("Could not find model from treeview");
        return;
    }

    gtk_list_store_clear(store);

    // Populate window
    for (book_list_iterator = books_data; book_list_iterator != NULL; book_list_iterator
            = book_list_iterator->next) {
        book_data = (book_data_t *) book_list_iterator->data;
        gtk_list_store_append(store, &list_store_iterator);
        g_debug("-----------------------------------: %s, %s", book_data->name, book_data->active ? "active" : "not-active");
        gtk_list_store_set(store, &list_store_iterator, COLUMN_BOOK_ACTIVE,
                           book_data->active, COLUMN_BOOK_UID, book_data->uid, COLUMN_BOOK_NAME,
                           book_data->name, -1);
    }
}

GtkWidget*
create_addressbook_settings(SFLPhoneClient *client)
{
    GtkWidget *result_frame, *value, *item;

    GtkListStore *store;
    GtkCellRenderer *renderer;
    GtkTreeViewColumn *tree_view_column;

    // Load the default values
    addressbook_config = addressbook_config_load_parameters(client->settings);

    GtkWidget *ret = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

    GtkWidget *grid;
    gnome_main_section_new_with_grid(_("General"), &result_frame, &grid);
    gtk_box_pack_start(GTK_BOX(ret), result_frame, FALSE, FALSE, 0);

    // PHOTO DISPLAY
    item = gtk_check_button_new_with_mnemonic(_("_Use Evolution address books"));
    g_signal_connect(G_OBJECT(item), "clicked", G_CALLBACK(enable_cb), client);
    /* 2x1 */
    gtk_grid_attach(GTK_GRID(grid), item, 1, 0, 2, 1);

    // SCALE BUTTON - NUMBER OF RESULTS
    scale_button = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    scale_label = gtk_label_new(_("Download limit :"));
    gtk_box_pack_start(GTK_BOX(scale_button),scale_label,FALSE,FALSE,0);
    value = gtk_spin_button_new_with_range(1, G_MAXINT, 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL(scale_label), value);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(value) , addressbook_config->max_results);
    g_signal_connect(G_OBJECT(value) , "value-changed" , G_CALLBACK(max_results_cb), NULL);
    gtk_box_pack_start(GTK_BOX(scale_button),value,TRUE,TRUE,10);
    gtk_grid_attach(GTK_GRID(grid), scale_button, 1, 1, 1, 1);
    cards_label = gtk_label_new(_("cards"));
    gtk_grid_attach(GTK_GRID(grid), cards_label, 2, 1, 1, 1);
    gtk_widget_show_all(scale_button);

    // PHOTO DISPLAY
    photo = gtk_check_button_new_with_mnemonic(_("_Display contact photo if available"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(photo), addressbook_config->display_contact_photo);
    g_signal_connect(G_OBJECT(photo) , "clicked" , G_CALLBACK(display_contact_photo_cb), NULL);
    /* 2x1 */
    gtk_grid_attach(GTK_GRID(grid), photo, 1, 2, 2, 1);

    // Fields
    gnome_main_section_new_with_grid(_("Fields from Evolution's address books"), &result_frame, &grid);
    gtk_box_pack_start(GTK_BOX(ret), result_frame, FALSE, FALSE, 0);

    business = gtk_check_button_new_with_mnemonic(_("_Work"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(business), addressbook_config->search_phone_business);
    g_signal_connect(G_OBJECT(business) , "clicked" , G_CALLBACK(search_phone_business_cb) , NULL);
    gtk_grid_attach(GTK_GRID(grid), business, 0, 0, 1, 1);
    gtk_widget_set_sensitive(business, FALSE);

    home = gtk_check_button_new_with_mnemonic(_("_Home"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(home), addressbook_config->search_phone_home);
    g_signal_connect(G_OBJECT(home) , "clicked" , G_CALLBACK(search_phone_home_cb) , NULL);
    gtk_grid_attach(GTK_GRID(grid), home, 0, 1, 1, 1);
    gtk_widget_set_sensitive(home, FALSE);

    mobile = gtk_check_button_new_with_mnemonic(_("_Mobile"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mobile), addressbook_config->search_phone_mobile);
    g_signal_connect(G_OBJECT(mobile) , "clicked" , G_CALLBACK(search_phone_mobile_cb) , NULL);
    gtk_grid_attach(GTK_GRID(grid), mobile, 0, 2, 1, 1);

    // Address Book
    gnome_main_section_new_with_grid(_("Address Books"), &result_frame, &grid);
    gtk_box_pack_start(GTK_BOX(ret), result_frame, TRUE, TRUE, 0);
    gtk_widget_show(result_frame);

    scrolled_label = gtk_label_new(_("Select which Evolution address books to use"));
    gtk_misc_set_alignment(GTK_MISC(scrolled_label), 0.00, 0.2);

    /* 3x1 */
    gtk_grid_attach(GTK_GRID(grid), scrolled_label, 1, 1, 3, 1);
    gtk_widget_set_sensitive(scrolled_label, FALSE);

    scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), GTK_SHADOW_IN);

    /* 3x1 */
    gtk_grid_attach(GTK_GRID(grid), scrolled_window, 1, 2, 3, 1);

    store = gtk_list_store_new(3,
                               G_TYPE_BOOLEAN,             // Active
                               G_TYPE_STRING,             // uid
                               G_TYPE_STRING              // Name
                              );

    // Create tree view with list store
    book_tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));

    // Active column
    renderer = gtk_cell_renderer_toggle_new();
    tree_view_column = gtk_tree_view_column_new_with_attributes("", renderer, "active", COLUMN_BOOK_ACTIVE, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(book_tree_view), tree_view_column);

    // Toggle active property on clicked
    g_signal_connect(G_OBJECT(renderer), "toggled", G_CALLBACK(addressbook_config_book_active_toggled), (gpointer) book_tree_view);

    // Name column
    renderer = gtk_cell_renderer_text_new();
    tree_view_column = gtk_tree_view_column_new_with_attributes(_("Name"), renderer, "markup", COLUMN_BOOK_NAME, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(book_tree_view), tree_view_column);

    g_object_unref(G_OBJECT(store));
    gtk_container_add(GTK_CONTAINER(scrolled_window), book_tree_view);

    addressbook_config_fill_book_list();

    gtk_widget_show_all(ret);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(item), addressbook_config->enable);

    enable_options();

    return ret;
}
