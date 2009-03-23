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

#include "hooks-config.h"

URLHook_Config *_urlhook_config;

GtkWidget *field, *command;

void hooks_load_parameters (URLHook_Config** settings){

    GHashTable *_params = NULL;
    URLHook_Config *_settings;

    // Allocate a struct
    _settings = g_new0 (URLHook_Config, 1);
    
    // Fetch the settings from D-Bus
    _params = (GHashTable*) dbus_get_hook_settings ();

    if (_params == NULL) {
        _settings->sip_field = DEFAULT_SIP_URL_FIELD;
        _settings->command = DEFAULT_URL_COMMAND;
    }
    else {
        _settings->sip_field =  (gchar*)(g_hash_table_lookup (_params, URLHOOK_SIP_FIELD));
        _settings->command =  (gchar*)(g_hash_table_lookup (_params, URLHOOK_COMMAND));
    }
 
    *settings = _settings;
}


void hooks_save_parameters (void){

    GHashTable *params = NULL;
    
    params = g_hash_table_new (NULL, g_str_equal);
    g_hash_table_replace (params, (gpointer)URLHOOK_SIP_FIELD, 
                                g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(field))));
    g_hash_table_replace (params, (gpointer)URLHOOK_COMMAND, 
                                g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(command))));
    
    dbus_set_hook_settings (params);

    // Decrement the reference count
    g_hash_table_unref (params);

}


GtkWidget* create_hooks_settings (){

    GtkWidget *ret, *url_frame, *table, *label;

    // Load the user value
    hooks_load_parameters (&_urlhook_config);

    ret = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

    url_frame = gtk_frame_new(_("URL argument"));
    gtk_box_pack_start(GTK_BOX(ret), url_frame, FALSE, FALSE, 0);
    gtk_widget_show (url_frame);

    table = gtk_table_new ( 5, 2,  FALSE/* homogeneous */);
    gtk_table_set_row_spacings( GTK_TABLE(table), 10);
    gtk_table_set_col_spacings( GTK_TABLE(table), 10);
    gtk_widget_show(table);
    gtk_container_add( GTK_CONTAINER (url_frame) , table );

    field = gtk_check_button_new_with_mnemonic( _("_SIP protocol"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(field), TRUE);
    g_signal_connect (G_OBJECT(field) , "clicked" , NULL, NULL);
    gtk_table_attach ( GTK_TABLE( table ), field, 0, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
 
    field = gtk_check_button_new_with_mnemonic( _("_IAX2 protocol"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(field), TRUE);
    g_signal_connect (G_OBJECT(field) , "clicked" , NULL, NULL);
    gtk_table_attach ( GTK_TABLE( table ), field, 0, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
 

    label = gtk_label_new_with_mnemonic (_("_SIP Field name: "));
    gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    field = gtk_entry_new ();
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), field);
    gtk_entry_set_text(GTK_ENTRY(field), _urlhook_config->sip_field);
    gtk_table_attach ( GTK_TABLE( table ), field, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    label = gtk_label_new_with_mnemonic (_("_Command: "));
    gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    command = gtk_entry_new ();
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), command);
    gtk_entry_set_text(GTK_ENTRY(command), _urlhook_config->command);
    gtk_table_attach ( GTK_TABLE( table ), command, 1, 2, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    gtk_widget_show_all(ret);

    return ret;
}
