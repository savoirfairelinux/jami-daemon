/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "addressbook-config.h"

AddressBook_Config *addressbook_config;

void addressbook_load_parameters (AddressBook_Config **settings) {

    GHashTable *_params = NULL;
    AddressBook_Config *_settings;

    // Allocate a struct
    _settings = g_new0 (AddressBook_Config, 1);

    // Fetch the settings from D-Bus
    _params = (GHashTable*) dbus_get_addressbook_settings ();

    if (_params == NULL) {
        _settings->max_results = 30;
        _settings->display_contact_photo = 0;
        _settings->search_phone_business = 1;
        _settings->search_phone_home = 1;
        _settings->search_phone_mobile = 1;
    }
    else {
        _settings->max_results =  (guint)(g_hash_table_lookup (_params, ADDRESSBOOK_MAX_RESULTS));
        _settings->display_contact_photo = (guint) (g_hash_table_lookup (_params, ADDRESSBOOK_DISPLAY_CONTACT_PHOTO));
        _settings->search_phone_business = (guint) (g_hash_table_lookup (_params, ADDRESSBOOK_DISPLAY_PHONE_BUSINESS));
        _settings->search_phone_home = (guint) (g_hash_table_lookup (_params, ADDRESSBOOK_DISPLAY_PHONE_HOME));
        _settings->search_phone_mobile = (guint) (g_hash_table_lookup (_params, ADDRESSBOOK_DISPLAY_PHONE_MOBILE));
    }

    *settings = _settings;
}

void addressbook_save_parameters (void) {

    GHashTable *params = NULL;

    params = g_hash_table_new (NULL, g_str_equal);
    g_hash_table_replace (params, (gpointer)ADDRESSBOOK_MAX_RESULTS, (gpointer)addressbook_config->max_results);
    g_hash_table_replace (params, (gpointer)ADDRESSBOOK_DISPLAY_CONTACT_PHOTO, (gpointer)addressbook_config->display_contact_photo);
    g_hash_table_replace (params, (gpointer)ADDRESSBOOK_DISPLAY_PHONE_BUSINESS, (gpointer)addressbook_config->search_phone_business);
    g_hash_table_replace (params, (gpointer)ADDRESSBOOK_DISPLAY_PHONE_HOME, (gpointer)addressbook_config->search_phone_home);
    g_hash_table_replace (params, (gpointer)ADDRESSBOOK_DISPLAY_PHONE_MOBILE, (gpointer)addressbook_config->search_phone_mobile);

    dbus_set_addressbook_settings (params);

    // Decrement the reference count
    g_hash_table_unref (params);
}

static void max_results_cb (GtkRange* scale) {

    addressbook_config->max_results = (guint) gtk_range_get_value (GTK_RANGE (scale));
}

static void display_contact_photo_cb (GtkWidget *widget) {

    addressbook_config->display_contact_photo = (guint) gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
}

static void search_phone_business_cb (GtkWidget *widget) {

    addressbook_config->search_phone_business = (guint) gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
}

static void search_phone_home_cb (GtkWidget *widget) {

    addressbook_config->search_phone_home = (guint) gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
}

static void search_phone_mobile_cb (GtkWidget *widget) {

    addressbook_config->search_phone_mobile = (guint) gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
}

GtkWidget* create_addressbook_settings () {

    GtkWidget *ret, *result_frame, *table, *value, *label, *photo, *item;

    // Load the user value
    addressbook_load_parameters (&addressbook_config);

    ret = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

    result_frame = gtk_frame_new(_("Search Parameters"));
    gtk_box_pack_start(GTK_BOX(ret), result_frame, FALSE, FALSE, 0);
    gtk_widget_show (result_frame);

    table = gtk_table_new ( 5, 3,  FALSE/* homogeneous */);
    gtk_table_set_row_spacings( GTK_TABLE(table), 10);
    gtk_table_set_col_spacings( GTK_TABLE(table), 10);
    gtk_widget_show(table);
    gtk_container_add( GTK_CONTAINER (result_frame) , table );

    // SCALE BUTTON - NUMBER OF RESULTS
    label = gtk_label_new (_("Maximum result number for a request: "));
    gtk_table_attach ( GTK_TABLE( table ), label, 0, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 10);
    value = gtk_hscale_new_with_range (25.0 , 50.0 , 5.0);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), value);
    gtk_scale_set_digits (GTK_SCALE(value) , 0);
    gtk_scale_set_value_pos (GTK_SCALE(value) , GTK_POS_RIGHT);
    gtk_range_set_value (GTK_RANGE( value ) , addressbook_config->max_results);
    g_signal_connect (G_OBJECT (value) , "value-changed" , G_CALLBACK(max_results_cb), NULL );
    gtk_table_attach ( GTK_TABLE( table ), value, 2, 3, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    // PHOTO DISPLAY
    photo = gtk_check_button_new_with_mnemonic( _("_Display contact photo if available"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(photo), addressbook_config->display_contact_photo);
    g_signal_connect (G_OBJECT(photo) , "clicked" , G_CALLBACK (display_contact_photo_cb), NULL);
    gtk_table_attach ( GTK_TABLE( table ), photo, 0, 3, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    label = gtk_label_new (_("Search for and display: "));
    gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    item = gtk_check_button_new_with_mnemonic( _("_Business phone"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(item), addressbook_config->search_phone_business);
    g_signal_connect (G_OBJECT(item) , "clicked" , G_CALLBACK (search_phone_business_cb) , NULL);
    gtk_table_attach ( GTK_TABLE( table ), item, 1, 3, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    item = gtk_check_button_new_with_mnemonic( _("_Home phone"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(item), addressbook_config->search_phone_home);
    g_signal_connect (G_OBJECT(item) , "clicked" , G_CALLBACK (search_phone_home_cb) , NULL);
    gtk_table_attach ( GTK_TABLE( table ), item, 1, 3, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    item = gtk_check_button_new_with_mnemonic( _("_Mobile phone"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(item), addressbook_config->search_phone_mobile);
    g_signal_connect (G_OBJECT(item) , "clicked" , G_CALLBACK (search_phone_mobile_cb) , NULL);
    gtk_table_attach ( GTK_TABLE( table ), item, 1, 3, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 10);

    gtk_widget_show_all(ret);

    return ret;

}

gboolean addressbook_display (AddressBook_Config *settings, const gchar *field) {

    gboolean display = FALSE;

    if (g_strcasecmp (field, ADDRESSBOOK_DISPLAY_CONTACT_PHOTO) == 0)
        display = (settings->display_contact_photo == 1)? TRUE : FALSE;

    else if (g_strcasecmp (field, ADDRESSBOOK_DISPLAY_PHONE_BUSINESS) == 0)
        display = (settings->search_phone_business == 1)? TRUE : FALSE;

    else if (g_strcasecmp (field, ADDRESSBOOK_DISPLAY_PHONE_HOME) == 0)
        display = (settings->search_phone_home == 1)? TRUE : FALSE;

    else if (g_strcasecmp (field, ADDRESSBOOK_DISPLAY_PHONE_MOBILE) == 0)
        display = (settings->search_phone_mobile == 1)? TRUE : FALSE;

    else
        display = FALSE;

    return display;
}
