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

GtkWidget *field, *command, *prefix;

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
        _settings->sip_enabled = "0";
        _settings->iax2_enabled = "0";
        _settings->phone_number_enabled = "0";
        _settings->phone_number_prefix = "";
    }
    else {
        _settings->sip_field =  (gchar*)(g_hash_table_lookup (_params, URLHOOK_SIP_FIELD));
        _settings->command =  (gchar*)(g_hash_table_lookup (_params, URLHOOK_COMMAND));
        _settings->sip_enabled =  (gchar*)(g_hash_table_lookup (_params, URLHOOK_SIP_ENABLED));
        _settings->iax2_enabled =  (gchar*)(g_hash_table_lookup (_params, URLHOOK_IAX2_ENABLED));
        _settings->phone_number_enabled =  (gchar*)(g_hash_table_lookup (_params, PHONE_NUMBER_HOOK_ENABLED ));
        _settings->phone_number_prefix =  (gchar*)(g_hash_table_lookup (_params, PHONE_NUMBER_HOOK_ADD_PREFIX ));
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
    g_hash_table_replace (params, (gpointer)URLHOOK_SIP_ENABLED, 
                                (gpointer)g_strdup(_urlhook_config->sip_enabled));
    g_hash_table_replace (params, (gpointer)URLHOOK_IAX2_ENABLED, 
                                (gpointer)g_strdup(_urlhook_config->iax2_enabled));
    g_hash_table_replace (params, (gpointer)PHONE_NUMBER_HOOK_ENABLED, 
                                (gpointer)g_strdup(_urlhook_config->phone_number_enabled));
    g_hash_table_replace (params, (gpointer)PHONE_NUMBER_HOOK_ADD_PREFIX, 
                                g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(prefix)))); 
    
    dbus_set_hook_settings (params);

    // Decrement the reference count
    g_hash_table_unref (params);

}

static void sip_enabled_cb (GtkWidget *widget) {

    guint check;

    check = (guint) gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
    if (check)
        _urlhook_config->sip_enabled="1";
    else
        _urlhook_config->sip_enabled="0";
}

static void iax2_enabled_cb (GtkWidget *widget) {

    guint check;

    check = (guint) gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
    if (check)
        _urlhook_config->iax2_enabled="1";
    else
        _urlhook_config->iax2_enabled="0";
}

static void phone_number_enabled_cb (GtkWidget *widget) {

    guint check;

    check = (guint) gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON(widget));
    if (check){
        _urlhook_config->phone_number_enabled="1";
        gtk_widget_set_sensitive (GTK_WIDGET (prefix), TRUE);  
    }else{
        _urlhook_config->phone_number_enabled="0";
        gtk_widget_set_sensitive (GTK_WIDGET (prefix), FALSE);  
    }
}


GtkWidget* create_hooks_settings (){

    GtkWidget *ret, *frame, *table, *label, *widg;

    // Load the user value
    hooks_load_parameters (&_urlhook_config);

    ret = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

    gnome_main_section_new_with_table (_("URL Passing"), &frame, &table, 5, 2);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);
    gtk_widget_show (frame);


    label = gtk_label_new(_("Custom commands on incoming calls with URL,"));
    gtk_table_attach ( GTK_TABLE( table ), label, 0, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    label = gtk_label_new(_("%s will be replaced with the passed URL."));
    gtk_table_attach ( GTK_TABLE( table ), label, 0, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    widg = gtk_check_button_new_with_mnemonic( _("Trigger on specific _SIP header"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widg), (g_strcasecmp (_urlhook_config->sip_enabled, "1")==0)?TRUE:FALSE);
    g_signal_connect (G_OBJECT(widg) , "clicked" , G_CALLBACK (sip_enabled_cb), NULL);
    gtk_table_attach ( GTK_TABLE( table ), widg, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
 
    field = gtk_entry_new ();
    gtk_entry_set_text(GTK_ENTRY(field), _urlhook_config->sip_field);
    gtk_table_attach ( GTK_TABLE( table ), field, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    widg = gtk_check_button_new_with_mnemonic( _("Trigger on _IAX2 URL"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widg), (g_strcasecmp (_urlhook_config->iax2_enabled, "1")==0)?TRUE:FALSE); 
    g_signal_connect (G_OBJECT(widg) , "clicked" , G_CALLBACK (iax2_enabled_cb), NULL);
    gtk_table_attach ( GTK_TABLE( table ), widg, 0, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    label = gtk_label_new_with_mnemonic (_("Command to _run: "));
    gtk_misc_set_alignment(GTK_MISC(label), 0.05, 0.5);
    gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    command = gtk_entry_new ();
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), command);
    gtk_entry_set_text(GTK_ENTRY(command), _urlhook_config->command);
    gtk_table_attach ( GTK_TABLE( table ), command, 1, 2, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 10);



    gnome_main_section_new_with_table (_("Phone Number Rewriting"), &frame, &table, 4, 2);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);
    gtk_widget_show (frame);

    widg = gtk_check_button_new_with_mnemonic( _("_Prefix dialed numbers with:"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON(widg), (g_strcasecmp (_urlhook_config->phone_number_enabled, "1")==0)?TRUE:FALSE);
    g_signal_connect (G_OBJECT(widg) , "clicked" , G_CALLBACK (phone_number_enabled_cb), NULL);
    gtk_table_attach ( GTK_TABLE( table ), widg, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
 
    prefix = gtk_entry_new ();
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), prefix);
    gtk_entry_set_text(GTK_ENTRY(prefix), _urlhook_config->phone_number_prefix);
    gtk_widget_set_sensitive (GTK_WIDGET (prefix), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON (widg)));
    gtk_table_attach ( GTK_TABLE( table ), prefix, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 10);

    gtk_widget_show_all(ret);

    return ret;
}
