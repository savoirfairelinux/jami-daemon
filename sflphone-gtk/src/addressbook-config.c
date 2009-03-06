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
    }
    else {
        _settings->max_results =  (guint)(g_hash_table_lookup (_params, "ADDRESSBOOK_MAX_RESULTS"));
        _settings->display_contact_photo = (guint) (g_hash_table_lookup (_params, "ADDRESSBOOK_DISPLAY_CONTACT_PHOTO"));
    }
    
    *settings = _settings;
}

static void max_results_cb (GtkRange* scale, gpointer user_data) {

    AddressBook_Config *settings = (AddressBook_Config*)user_data;
    settings->max_results = (guint) gtk_range_get_value (GTK_RANGE (scale));
}

static void display_contact_photo_cb (GtkWidget *widget, gpointer user_data) {

    AddressBook_Config *settings = (AddressBook_Config*)user_data;
    settings->display_contact_photo = (guint) gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
}


GtkWidget* create_addressbook_settings () {
    
    GtkWidget *ret, *result_frame, *box, *value, *label, *photo;
    AddressBook_Config *settings;

    // Load the user value
    addressbook_load_parameters (&settings);

    ret = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

    result_frame = gtk_frame_new(_("Search Parameters"));
    gtk_box_pack_start(GTK_BOX(ret), result_frame, FALSE, FALSE, 0);
    gtk_widget_show (result_frame);

    box = gtk_vbox_new( FALSE , 1);
    gtk_widget_show (box);
    gtk_container_add (GTK_CONTAINER(result_frame) , box);

    // SCALE BUTTON - NUMBER OF RESULTS
    label = gtk_label_new (_("Maximum result number for a request: "));
    gtk_box_pack_start (GTK_BOX(box) , label , FALSE , FALSE , 1);
    value = gtk_hscale_new_with_range (0.0 , 50.0 , 5.0);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), value);
    gtk_scale_set_digits (GTK_SCALE(value) , 0);
    gtk_scale_set_value_pos (GTK_SCALE(value) , GTK_POS_RIGHT); 
    gtk_range_set_value (GTK_RANGE( value ) , settings->max_results);
    gtk_box_pack_start (GTK_BOX(box) , value , TRUE , TRUE , 0);
    g_signal_connect (G_OBJECT (value) , "value-changed" , G_CALLBACK(max_results_cb) , settings);

    // PHOTO DISPLAY
    photo = gtk_check_button_new_with_mnemonic( _("_Display contact photo if available"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(photo), settings->display_contact_photo);
    g_signal_connect (G_OBJECT(photo) , "clicked" , G_CALLBACK (display_contact_photo_cb) , settings);
    gtk_box_pack_start (GTK_BOX(box) , photo , TRUE , TRUE , 1);
     
    gtk_widget_show_all(ret);

    return ret;

}
