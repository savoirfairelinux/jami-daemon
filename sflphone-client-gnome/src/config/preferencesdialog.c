/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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
 */

#include <gtk/gtk.h>

#include <accountlist.h>
#include <accountconfigdialog.h>
#include <actions.h>
#include <config.h>
#include <dbus/dbus.h>
#include <mainwindow.h>
#include <audioconf.h>
#include <addressbook-config.h>
#include <hooks-config.h>
#include <utils.h>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

/**
 * Local variables
 */
gboolean accDialogOpen = FALSE;
gboolean dialogOpen = FALSE;
gboolean ringtoneEnabled = TRUE;

GtkWidget * applyButton;
GtkWidget * history_value;

GtkWidget * status;

static int history_limit;
static gboolean history_enabled = TRUE;


GHashTable * directIpCallsProperties = NULL;

static void update_port_cb ( GtkSpinButton *button UNUSED, void *ptr )
{
    dbus_set_sip_port(gtk_spin_button_get_value_as_int((GtkSpinButton *)(ptr)));
}


static void
set_md5_hash_cb(GtkWidget *widget UNUSED, gpointer data UNUSED)
{
    gboolean enabled = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
    dbus_set_md5_credential_hashing(enabled);
}

static void
start_hidden( void )
{
    dbus_start_hidden();
}

static void
set_popup_mode( void )
{
    dbus_switch_popup_mode();
}


    void
set_notif_level(  )
{
    dbus_set_notify();
}

static void history_limit_cb (GtkSpinButton *button, void *ptr)
{
    history_limit = gtk_spin_button_get_value_as_int((GtkSpinButton *)(ptr));
}

static void history_enabled_cb (GtkWidget *widget)
{
    history_enabled = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (widget));
    gtk_widget_set_sensitive (GTK_WIDGET (history_value), history_enabled);
        
    // Toggle it through D-Bus
    dbus_set_history_enabled ();
}
    
	void
clean_history( void )
{
    calllist_clean_history();
}

static void show_advanced_zrtp_options_cb(GtkWidget *widget UNUSED, gpointer data)
{
    DEBUG("Advanced options for ZRTP");
    show_advanced_zrtp_options((GHashTable *) data);
}

static void show_advanced_tls_options_cb(GtkWidget *widget UNUSED, gpointer data)
{
    DEBUG("Advanced options for TLS");
    show_advanced_tls_options((GHashTable *) data);
}

static void key_exchange_changed_cb(GtkWidget *widget, gpointer data)
{
    DEBUG("Key exchange changed");
    if (g_strcasecmp(gtk_combo_box_get_active_text(GTK_COMBO_BOX(widget)), (gchar *) "ZRTP") == 0) {
        gtk_widget_set_sensitive(GTK_WIDGET(data), TRUE);
	g_hash_table_replace(directIpCallsProperties, g_strdup(ACCOUNT_SRTP_ENABLED), g_strdup("true"));
        g_hash_table_replace(directIpCallsProperties, g_strdup(ACCOUNT_KEY_EXCHANGE), g_strdup(ZRTP));
    } else {
        gtk_widget_set_sensitive(GTK_WIDGET(data), FALSE);
        DEBUG("Setting key exchange %s to %s\n", ACCOUNT_KEY_EXCHANGE, KEY_EXCHANGE_NONE);
	g_hash_table_replace(directIpCallsProperties, g_strdup(ACCOUNT_SRTP_ENABLED), g_strdup("false"));
        g_hash_table_replace(directIpCallsProperties, g_strdup(ACCOUNT_KEY_EXCHANGE), g_strdup(KEY_EXCHANGE_NONE));
    }
}

static void use_sip_tls_cb(GtkWidget *widget, gpointer data)
{
    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
        DEBUG("Using sips");
    	gtk_widget_set_sensitive(GTK_WIDGET(data), TRUE);  
        g_hash_table_replace(directIpCallsProperties,
				g_strdup(TLS_ENABLE), g_strdup("true"));    	          
    } else {
        gtk_widget_set_sensitive(GTK_WIDGET(data), FALSE);
        g_hash_table_replace(directIpCallsProperties,
				g_strdup(TLS_ENABLE), g_strdup("false"));             
    }   
}

GtkWidget* create_direct_ip_calls_tab()
{
    GtkWidget * frame;
    GtkWidget * table;
    GtkWidget * label;
    GtkWidget * explanationLabel;

    GtkWidget * localPortLabel;
    GtkWidget * localPortSpinBox;
    GtkWidget * localAddressLabel;
    GtkWidget * localAddressCombo;

    GtkWidget * keyExchangeCombo;
    GtkWidget * advancedZrtpButton;
    GtkWidget * useSipTlsCheckBox;  
    
    gchar * curSRTPEnabled = "false";
    gchar * curTlsEnabled = "false";    
    gchar * curKeyExchange = "0";
    gchar * description;
   
    //directIpCallsProperties = sflphone_get_ip2ip_properties();
    sflphone_get_ip2ip_properties(&directIpCallsProperties);
              
    if(directIpCallsProperties != NULL) {
	DEBUG("got a directIpCallsProperties");
        curSRTPEnabled = g_hash_table_lookup(directIpCallsProperties, ACCOUNT_SRTP_ENABLED);
	DEBUG("    curSRTPEnabled = %s", curSRTPEnabled);
        curKeyExchange = g_hash_table_lookup(directIpCallsProperties, ACCOUNT_KEY_EXCHANGE);
        curTlsEnabled = g_hash_table_lookup(directIpCallsProperties, TLS_ENABLE);        
    }
                
	GtkWidget * vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    description = g_markup_printf_escaped(_("This profile is used when you want to reach a remote peer simply by typing a sip URI such as <b>sip:remotepeer</b>. The settings you define here will also be used if no account can be matched to an incoming or outgoing call."));
    explanationLabel = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(explanationLabel), description);
 	gtk_misc_set_alignment(GTK_MISC(explanationLabel), 0, 0.5);    
    gtk_box_pack_start(GTK_BOX(vbox), explanationLabel, FALSE, FALSE, 0);

    /**
     * Network Interface Section 
     */
    gnome_main_section_new_with_table (_("Network Interface"), &frame, &table, 2, 2);
    gtk_container_set_border_width (GTK_CONTAINER(table), 10);
    gtk_table_set_row_spacings (GTK_TABLE(table), 10);
    gtk_table_set_col_spacings( GTK_TABLE(table), 10);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    /**
     * Retreive the list of IP interface from the 
     * the daemon and build the combo box.
     */
    
    GtkListStore * ipInterfaceListStore; 
    GtkTreeIter iter;
    
    ipInterfaceListStore =  gtk_list_store_new( 1, G_TYPE_STRING );
    localAddressLabel = gtk_label_new_with_mnemonic (_("Local address"));    
    gtk_table_attach ( GTK_TABLE( table ), localAddressLabel, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC (localAddressLabel), 0, 0.5);
			
    GtkTreeIter current_local_address_iter = iter;   
    gchar ** iface_list = NULL;
    iface_list = (gchar**) dbus_get_all_ip_interface();
    gchar ** iface = NULL;
    
    if (iface_list != NULL) {
      for (iface = iface_list; *iface; iface++) {         
	DEBUG("Interface %s", *iface);            
	gtk_list_store_append(ipInterfaceListStore, &iter );
	gtk_list_store_set(ipInterfaceListStore, &iter, 0, *iface, -1 );
	
	// if (g_strcmp0(*iface, local_address) == 0) {
	// DEBUG("Setting active local address combo box");
	current_local_address_iter = iter;
	// }
      }
    }
    
    localAddressCombo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(ipInterfaceListStore));
    gtk_label_set_mnemonic_widget(GTK_LABEL(localAddressLabel), localAddressCombo);
    gtk_table_attach ( GTK_TABLE( table ), localAddressCombo, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    g_object_unref(G_OBJECT(ipInterfaceListStore));	

    GtkCellRenderer * ipInterfaceCellRenderer;
    ipInterfaceCellRenderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(localAddressCombo), ipInterfaceCellRenderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(localAddressCombo), ipInterfaceCellRenderer, "text", 0, NULL);
    gtk_combo_box_set_active_iter(GTK_COMBO_BOX(localAddressCombo), &current_local_address_iter);


    /**
     * Local port
     */	    
    /** SIP port information */
    int local_port = dbus_get_sip_port();
    if(local_port <= 0 || local_port > 65535) {
        local_port = 5060; 
    }

    GtkWidget *applySipPortButton = gtk_button_new_from_stock(GTK_STOCK_APPLY);

    localPortLabel = gtk_label_new_with_mnemonic (_("Local port"));
    gtk_table_attach_defaults(GTK_TABLE(table), localPortLabel, 0, 1, 1, 2);

    gtk_misc_set_alignment(GTK_MISC (localPortLabel), 0, 0.5);
    localPortSpinBox = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_label_set_mnemonic_widget (GTK_LABEL (localPortLabel), localPortSpinBox); 
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(localPortSpinBox), local_port);
    g_signal_connect( G_OBJECT( applySipPortButton) , "clicked" , G_CALLBACK( update_port_cb ) , applySipPortButton);
    
    gtk_table_attach_defaults(GTK_TABLE(table), localPortSpinBox, 1, 2, 1, 2);

    /**
     * Security Section 
     */
    gnome_main_section_new_with_table (_("Security"), &frame, &table, 2, 3);
	gtk_container_set_border_width (GTK_CONTAINER(table), 10);
	gtk_table_set_row_spacings (GTK_TABLE(table), 10);
    gtk_table_set_col_spacings( GTK_TABLE(table), 10);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

	GtkWidget * sipTlsAdvancedButton;
	sipTlsAdvancedButton = gtk_button_new_from_stock(GTK_STOCK_EDIT);
    gtk_table_attach_defaults(GTK_TABLE(table), sipTlsAdvancedButton, 2, 3, 0, 1);
	gtk_widget_set_sensitive(GTK_WIDGET(sipTlsAdvancedButton), FALSE);    
    g_signal_connect(G_OBJECT(sipTlsAdvancedButton), "clicked", G_CALLBACK(show_advanced_tls_options_cb), directIpCallsProperties);
    
	useSipTlsCheckBox = gtk_check_button_new_with_mnemonic(_("Use TLS transport (sips)"));
	g_signal_connect (useSipTlsCheckBox, "toggled", G_CALLBACK(use_sip_tls_cb), sipTlsAdvancedButton);
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(useSipTlsCheckBox), (g_strcmp0(curTlsEnabled, "false") == 0) ? FALSE:TRUE);
	gtk_table_attach_defaults(GTK_TABLE(table), useSipTlsCheckBox, 0, 2, 0, 1);
       	    
    label = gtk_label_new_with_mnemonic (_("SRTP key exchange"));
 	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    keyExchangeCombo = gtk_combo_box_new_text();
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), keyExchangeCombo);
    gtk_combo_box_append_text(GTK_COMBO_BOX(keyExchangeCombo), "ZRTP");
    //gtk_combo_box_append_text(GTK_COMBO_BOX(keyExchangeCombo), "SDES");
    gtk_combo_box_append_text(GTK_COMBO_BOX(keyExchangeCombo), _("Disabled"));      
    
    advancedZrtpButton = gtk_button_new_from_stock(GTK_STOCK_PREFERENCES);
    g_signal_connect(G_OBJECT(advancedZrtpButton), "clicked", G_CALLBACK(show_advanced_zrtp_options_cb), directIpCallsProperties);
    
    if (g_strcasecmp(curKeyExchange, ZRTP) == 0) {
        gtk_combo_box_set_active(GTK_COMBO_BOX(keyExchangeCombo),0);
    } else {
        gtk_combo_box_set_active(GTK_COMBO_BOX(keyExchangeCombo), 1);
        gtk_widget_set_sensitive(GTK_WIDGET(advancedZrtpButton), FALSE);
    }
    
	g_signal_connect (G_OBJECT (GTK_COMBO_BOX(keyExchangeCombo)), "changed", G_CALLBACK (key_exchange_changed_cb), advancedZrtpButton);
    
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 1, 2);
    gtk_table_attach_defaults(GTK_TABLE(table), keyExchangeCombo, 1, 2, 1, 2);    
    gtk_table_attach_defaults(GTK_TABLE(table), advancedZrtpButton, 2, 3, 1, 2);
    
    gtk_widget_show_all(table);
        
    GtkRequisition requisition;
    gtk_widget_size_request(GTK_WIDGET(table), &requisition);
    gtk_widget_set_size_request(GTK_WIDGET(explanationLabel), requisition.width * 1.5, -1);        
    gtk_label_set_line_wrap(GTK_LABEL(explanationLabel), TRUE);
    
    gtk_widget_show_all(vbox);
    
    return vbox;
}

GtkWidget* create_network_tab()
{
    GtkWidget * frame;
    GtkWidget * table;
    GtkWidget * label;
    GtkWidget * ret;
    gchar * description;

    ret = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

    /** SIP port information */
    int curPort = dbus_get_sip_port();
    if(curPort <= 0 || curPort > 65535) {
        curPort = 5060; 
    }
    
    int account_number = account_list_get_sip_account_number();
    DEBUG("sip account number = %i", account_number);
    
    gnome_main_section_new_with_table (_("SIP Port"), &frame, &table, 1, 3);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);
    gtk_widget_set_sensitive(GTK_WIDGET(frame),(account_number == 0) ? FALSE:TRUE);

    GtkWidget *applySipPortButton = gtk_button_new_from_stock(GTK_STOCK_APPLY);
    GtkWidget *entryPort; 
    
    label = gtk_label_new(_("UDP Transport"));
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    entryPort = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryPort);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(entryPort), curPort);
    g_signal_connect( G_OBJECT( applySipPortButton) , "clicked" , G_CALLBACK( update_port_cb ) , entryPort);

    gtk_table_attach( GTK_TABLE(table), label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);
    gtk_table_attach( GTK_TABLE(table), entryPort, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);
    gtk_table_attach( GTK_TABLE(table), applySipPortButton, 2, 3, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);
    
    gtk_widget_show_all(ret);

    return ret;
}

    GtkWidget*
create_general_settings ()
{

    GtkWidget *ret;

    GtkWidget *notifAll;

    GtkWidget *trayItem;
    GtkWidget *frame;
    GtkWidget *checkBoxWidget;
    GtkWidget *label;
    GtkWidget *table;

    // Load history configuration
    history_load_configuration ();

    // Main widget
    ret = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);

    // Notifications Frame
    gnome_main_section_new_with_table (_("Desktop Notifications"), &frame, &table, 2, 1);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

    // Notification All
    notifAll = gtk_check_button_new_with_mnemonic( _("_Enable notifications"));
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(notifAll), dbus_get_notify() ); 
    g_signal_connect(G_OBJECT( notifAll ) , "clicked" , G_CALLBACK( set_notif_level ) , NULL );
    gtk_table_attach( GTK_TABLE(table), notifAll, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    // System Tray option frame
    gnome_main_section_new_with_table (_("System Tray Icon"), &frame, &table, 3, 1);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

    GtkWidget* trayItem1 = gtk_radio_button_new_with_mnemonic(NULL,  _("_Popup main window on incoming call"));
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(trayItem1), dbus_popup_mode() );
    g_signal_connect(G_OBJECT( trayItem1 ), "clicked", G_CALLBACK( set_popup_mode ) , NULL);
    gtk_table_attach( GTK_TABLE(table), trayItem1, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    trayItem = gtk_radio_button_new_with_mnemonic_from_widget(GTK_RADIO_BUTTON(trayItem1), _("Ne_ver popup main window"));
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(trayItem), !dbus_popup_mode() );
    gtk_table_attach( GTK_TABLE(table), trayItem, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    trayItem = gtk_check_button_new_with_mnemonic(_("Hide SFLphone window on _startup"));
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(trayItem), dbus_is_start_hidden() );
    g_signal_connect(G_OBJECT( trayItem ) , "clicked" , G_CALLBACK( start_hidden ) , NULL);
    gtk_table_attach( GTK_TABLE(table), trayItem, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    // HISTORY CONFIGURATION
    gnome_main_section_new_with_table (_("Calls History"), &frame, &table, 3, 1);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);

    checkBoxWidget = gtk_check_button_new_with_mnemonic(_("_Keep my history for at least"));
    gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (checkBoxWidget), history_enabled);
    g_signal_connect (G_OBJECT (checkBoxWidget) , "clicked" , G_CALLBACK (history_enabled_cb) , NULL);
    gtk_table_attach( GTK_TABLE(table), checkBoxWidget, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);
    
    history_value = gtk_spin_button_new_with_range(1, 99, 1);
    gtk_spin_button_set_value (GTK_SPIN_BUTTON(history_value), history_limit);
    g_signal_connect( G_OBJECT (history_value) , "value-changed" , G_CALLBACK (history_limit_cb) , history_value);
    gtk_widget_set_sensitive (GTK_WIDGET (history_value), gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (checkBoxWidget)));
    gtk_table_attach( GTK_TABLE(table), history_value, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5); 

    label = gtk_label_new(_("days"));
    gtk_table_attach( GTK_TABLE(table), label, 2, 3, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);
  
    // Configuration File
    gnome_main_section_new_with_table (_("Configuration File"), &frame, &table, 1, 1);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);    
    checkBoxWidget = gtk_check_button_new_with_mnemonic(_("Store SIP credentials as MD5 hash"));
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON(checkBoxWidget), dbus_is_md5_credential_hashing() );
    g_signal_connect(G_OBJECT( checkBoxWidget ) , "clicked" , G_CALLBACK(set_md5_hash_cb) , NULL);
    gtk_table_attach( GTK_TABLE(table), checkBoxWidget, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);
              
    gtk_widget_show_all(ret);

    return ret;
}

void save_configuration_parameters (void) {

    // Address book config
    addressbook_config_save_parameters ();
    hooks_save_parameters ();

    // History config
    dbus_set_history_limit (history_limit);
    
    // Direct IP calls config
    dbus_set_ip2ip_details(directIpCallsProperties);
}

void history_load_configuration ()
{
    history_limit = dbus_get_history_limit ();
    history_enabled = TRUE;
    if (g_strcasecmp (dbus_get_history_enabled (), "false") == 0)
        history_enabled = FALSE;
}

/**
 * Show configuration window with tabs
 */
    void
show_preferences_dialog ()
{
    GtkDialog * dialog;
    GtkWidget * notebook;
    GtkWidget * tab;
    guint result;

    dialogOpen = TRUE;

    dialog = GTK_DIALOG(gtk_dialog_new_with_buttons (_("Preferences"),
                GTK_WINDOW(get_main_window()),
                GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_STOCK_CLOSE,
                GTK_RESPONSE_ACCEPT,
                NULL));

    // Set window properties
    gtk_dialog_set_has_separator(dialog, FALSE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 600, 400);
    gtk_container_set_border_width(GTK_CONTAINER(dialog), 0);

    // Create tabs container
    notebook = gtk_notebook_new();
    gtk_box_pack_start(GTK_BOX (dialog->vbox), notebook, TRUE, TRUE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(notebook), 10);
    gtk_widget_show(notebook);

    // General settings tab
    tab = create_general_settings();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("General")));
    gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);

    // Audio tab
    tab = create_audio_configuration();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Audio")));
    gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);

    // Addressbook tab
    tab = create_addressbook_settings();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Address Book")));
    gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);

    // HookS tab
    tab = create_hooks_settings();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Hooks")));
    gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);

    // Network tab
    tab = create_network_tab();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Network")));
    gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);

    // Direct IP calls tab
    tab = create_direct_ip_calls_tab();
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab, gtk_label_new(_("Direct IP calls")));
    gtk_notebook_page_num(GTK_NOTEBOOK(notebook), tab);
        
    gtk_notebook_set_current_page( GTK_NOTEBOOK( notebook) ,  0);

    result = gtk_dialog_run(dialog);

    save_configuration_parameters ();
    update_actions();

    dialogOpen = FALSE;

    gtk_widget_destroy(GTK_WIDGET(dialog));
}

