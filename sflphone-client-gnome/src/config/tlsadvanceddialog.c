/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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

#include <tlsadvanceddialog.h>
#include <sflphone_const.h>
#include <utils.h>
#include <dbus.h>

#include <gtk/gtk.h>
#include <math.h>

void show_advanced_tls_options(GHashTable * properties)
{
    GtkDialog * tlsDialog;
    GtkWidget * image;
    GtkWidget * ret;
            
    tlsDialog = GTK_DIALOG(gtk_dialog_new_with_buttons (_("Advanced options for TLS"),
                GTK_WINDOW(get_main_window()),
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_STOCK_CANCEL,
                GTK_RESPONSE_CANCEL,
                GTK_STOCK_SAVE,
                GTK_RESPONSE_ACCEPT,
                NULL));
    gtk_window_set_policy( GTK_WINDOW(tlsDialog), FALSE, FALSE, FALSE );
    gtk_dialog_set_has_separator(tlsDialog, TRUE);
    gtk_container_set_border_width (GTK_CONTAINER(tlsDialog), 0);

    ret = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 10);
    gtk_box_pack_start(GTK_BOX(tlsDialog->vbox), ret, FALSE, FALSE, 0);  
            
    GtkWidget *frame, *table;     
    gnome_main_section_new_with_table (_("TLS transport"), &frame, &table, 3, 13);
    gtk_container_set_border_width(GTK_CONTAINER (table), 10);
    gtk_box_pack_start(GTK_BOX(ret), frame, FALSE, FALSE, 0);
        
    gchar * description = g_markup_printf_escaped(_("TLS transport can be used along with UDP for those calls that would\n"\
                                                    "require secure sip transactions (aka SIPS). You can configure a different\n"\
                                                    "TLS transport for each account. However, each of them will run on a dedicated\n"\
                                                    "port, different one from each other\n"));
    GtkWidget * label;                                         
    label = gtk_label_new(NULL);
 	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_label_set_markup(GTK_LABEL(label), description);
    gtk_table_attach(GTK_TABLE(table), label, 0, 3, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
            
    label = gtk_label_new(_("TLS listener port"));
 	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    GtkWidget * spinTlsPort;    
    spinTlsPort = gtk_spin_button_new_with_range(1, 65535, 1);
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), spinTlsPort);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(spinTlsPort), 5061);
    gtk_table_attach(GTK_TABLE(table), spinTlsPort, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    GHashTable * default_settings = NULL;
    gchar * tls_method = NULL;
    gchar * negotiation_timeout_sec = NULL;
    gchar * negotiation_timeout_msec = NULL; 
    gchar * require_client_certificate = NULL;
    gchar * verify_server = NULL;
    gchar * verify_client = NULL;
    
    default_settings = dbus_get_tls_settings_default();
    if (default_settings != NULL) {
	    tls_method = g_hash_table_lookup(default_settings, TLS_METHOD);
	    negotiation_timeout_sec = g_hash_table_lookup(default_settings, TLS_NEGOTIATION_TIMEOUT_SEC);
	    negotiation_timeout_msec = g_hash_table_lookup(default_settings, TLS_NEGOTIATION_TIMEOUT_MSEC);
	    require_client_certificate = g_hash_table_lookup(default_settings, TLS_REQUIRE_CLIENT_CERTIFICATE);	    	    
	    verify_server = g_hash_table_lookup(default_settings, TLS_VERIFY_SERVER);	    	    
	    verify_client = g_hash_table_lookup(default_settings, TLS_VERIFY_CLIENT);	    	    
    }
    
    label = gtk_label_new( _("Certificate of Authority list"));
 	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_table_attach (GTK_TABLE(table), label, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    GtkWidget * fileChooser;
    fileChooser = gtk_file_chooser_button_new(_("Choose a CA list file"), GTK_FILE_CHOOSER_ACTION_OPEN);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER( fileChooser) , g_get_home_dir());
    gtk_table_attach (GTK_TABLE(table), fileChooser, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
    label = gtk_label_new( _("Public endpoint certificate file"));
 	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_table_attach (GTK_TABLE(table), label, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    fileChooser = gtk_file_chooser_button_new(_("Choose a CA list file"), GTK_FILE_CHOOSER_ACTION_OPEN);
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER( fileChooser) , g_get_home_dir());
    gtk_table_attach (GTK_TABLE(table), fileChooser, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
 
	label = gtk_label_new_with_mnemonic (_("Certificate private key"));
 	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach (GTK_TABLE(table), label, 0, 1, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);	
	GtkWidget * privateKeyEntry;
#if GTK_CHECK_VERSION(2,16,0)
	privateKeyEntry = gtk_entry_new();
	gtk_entry_set_icon_from_stock (GTK_ENTRY (privateKeyEntry), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_DIALOG_AUTHENTICATION);
#else
	privateKeyEntry = sexy_icon_entry_new();
	image = gtk_image_new_from_stock(GTK_STOCK_DIALOG_AUTHENTICATION , GTK_ICON_SIZE_SMALL_TOOLBAR );
	sexy_icon_entry_set_icon(SEXY_ICON_ENTRY(privateKeyEntry), SEXY_ICON_ENTRY_PRIMARY , GTK_IMAGE(image) );
#endif
	gtk_entry_set_visibility(GTK_ENTRY(privateKeyEntry), FALSE);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), privateKeyEntry);
	//gtk_entry_set_text(GTK_ENTRY(privateKeyEntry), curPassword);
	gtk_table_attach (GTK_TABLE(table), privateKeyEntry, 1, 2, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
 
 	label = gtk_label_new_with_mnemonic (_("Password for the private key"));
 	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	gtk_table_attach (GTK_TABLE(table), label, 0, 1, 5, 6, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);  
 	GtkWidget * privateKeyPasswordEntry;
#if GTK_CHECK_VERSION(2,16,0)
	privateKeyPasswordEntry = gtk_entry_new();
	gtk_entry_set_icon_from_stock (GTK_ENTRY (privateKeyPasswordEntry), GTK_ENTRY_ICON_PRIMARY, GTK_STOCK_DIALOG_AUTHENTICATION);
#else
	privateKeyPasswordEntry = sexy_icon_entry_new();
	image = gtk_image_new_from_stock(GTK_STOCK_DIALOG_AUTHENTICATION , GTK_ICON_SIZE_SMALL_TOOLBAR );
	sexy_icon_entry_set_icon(SEXY_ICON_ENTRY(privateKeyPasswordEntry), SEXY_ICON_ENTRY_PRIMARY , GTK_IMAGE(image) );
#endif
	gtk_entry_set_visibility(GTK_ENTRY(privateKeyPasswordEntry), FALSE);
	gtk_label_set_mnemonic_widget (GTK_LABEL (label), privateKeyPasswordEntry);
	//gtk_entry_set_text(GTK_ENTRY(privateKeyEntry), curPassword);
	gtk_table_attach (GTK_TABLE(table), privateKeyPasswordEntry, 1, 2, 5, 6, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);  
   
    /* TLS protocol methods */    
    GtkListStore * tlsProtocolMethodListStore; 
    GtkTreeIter iter;
    GtkWidget * tlsProtocolMethodCombo;
    
    tlsProtocolMethodListStore =  gtk_list_store_new( 1, G_TYPE_STRING );
	label = gtk_label_new_with_mnemonic (_("TLS protocol method"));
	gtk_table_attach (GTK_TABLE(table), label, 0, 1, 6, 7, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
			
	gchar** supported_tls_method = NULL;
    supported_tls_method = dbus_get_supported_tls_method();
    GtkTreeIter supported_tls_method_iter = iter;
    if (supported_tls_method != NULL) {
        char **supported_tls_method_ptr;
        for (supported_tls_method_ptr = supported_tls_method; *supported_tls_method_ptr; supported_tls_method_ptr++) {
            DEBUG("Supported Method %s", *supported_tls_method_ptr);
            gtk_list_store_append(tlsProtocolMethodListStore, &iter );
            gtk_list_store_set(tlsProtocolMethodListStore, &iter, 0, *supported_tls_method_ptr, -1 );
            
            if (g_strcmp0(*supported_tls_method_ptr, tls_method) == 0) {
                DEBUG("Setting active element in TLS protocol combo box");
                supported_tls_method_iter = iter;
            }
        }
    }
    
    tlsProtocolMethodCombo = gtk_combo_box_new_with_model(GTK_TREE_MODEL(tlsProtocolMethodListStore));
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), tlsProtocolMethodCombo);
	gtk_table_attach(GTK_TABLE(table), tlsProtocolMethodCombo, 1, 2, 6, 7, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    g_object_unref(G_OBJECT(tlsProtocolMethodListStore));	
    
    GtkCellRenderer *tlsProtocolMethodCellRenderer;
    tlsProtocolMethodCellRenderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(tlsProtocolMethodCombo), tlsProtocolMethodCellRenderer, TRUE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(tlsProtocolMethodCombo), tlsProtocolMethodCellRenderer, "text", 0, NULL);
    gtk_combo_box_set_active_iter(GTK_COMBO_BOX(tlsProtocolMethodCombo), &supported_tls_method_iter);
	
	/* Cipher list */
    GtkWidget * cipherListEntry;
	label = gtk_label_new_with_mnemonic (_("TLS cipher list"));
	gtk_table_attach (GTK_TABLE(table), label, 0, 1, 7, 8, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	cipherListEntry = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), cipherListEntry);
	//gtk_entry_set_text(GTK_ENTRY(entryHostname), curHostname);
	gtk_table_attach (GTK_TABLE(table), cipherListEntry, 1, 2, 7, 8, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	
    GtkWidget * serverNameInstance;
	label = gtk_label_new_with_mnemonic (_("Server name instance for outgoing TLS connection"));
	gtk_table_attach (GTK_TABLE(table), label, 0, 1, 8, 9, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
	serverNameInstance = gtk_entry_new();
	gtk_label_set_mnemonic_widget(GTK_LABEL(label), serverNameInstance);
	//gtk_entry_set_text(GTK_ENTRY(entryHostname), curHostname);
	gtk_table_attach (GTK_TABLE(table), serverNameInstance, 1, 2, 8, 9, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    label = gtk_label_new(_("Negotiation timeout (sec:msec)"));
 	gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
    gtk_table_attach(GTK_TABLE(table), label, 0, 1, 9, 10, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    GtkWidget * tlsTimeOutSec;    
    GtkWidget * hbox = gtk_hbox_new(FALSE, 10);
    gtk_table_attach(GTK_TABLE(table), hbox, 1, 2, 9, 10, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    tlsTimeOutSec = gtk_spin_button_new_with_range(0, pow(2,sizeof(long)), 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL (label), tlsTimeOutSec);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(tlsTimeOutSec), g_ascii_strtod(negotiation_timeout_sec, NULL));
    gtk_box_pack_start_defaults(GTK_BOX(hbox), tlsTimeOutSec);   
    GtkWidget * tlsTimeOutMSec;    
    tlsTimeOutMSec = gtk_spin_button_new_with_range(0, pow(2,sizeof(long)), 1);
    gtk_label_set_mnemonic_widget(GTK_LABEL (label), tlsTimeOutMSec);
    gtk_spin_button_set_value(GTK_SPIN_BUTTON(tlsTimeOutMSec), g_ascii_strtod(negotiation_timeout_msec, NULL));
    gtk_box_pack_start_defaults(GTK_BOX(hbox), tlsTimeOutMSec);   
    
    GtkWidget * verifyCertificateServer;
	verifyCertificateServer = gtk_check_button_new_with_mnemonic(_("Verify incoming certificates, as a client"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(verifyCertificateServer),
			g_strcasecmp(verify_server,"TRUE") == 0 ? TRUE: FALSE);
	gtk_table_attach (GTK_TABLE(table), verifyCertificateServer, 0, 1, 10, 11, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    GtkWidget * verifyCertificateClient;
	verifyCertificateClient = gtk_check_button_new_with_mnemonic(_("Verify certificates from answer, as a server"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(verifyCertificateClient),
			g_strcasecmp(verify_client,"TRUE") == 0 ? TRUE: FALSE);
	gtk_table_attach (GTK_TABLE(table), verifyCertificateClient, 0, 1, 11, 12, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

    GtkWidget * requireCertificate;
	requireCertificate = gtk_check_button_new_with_mnemonic(_("Require certificate for incoming tls connections"));
	gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(requireCertificate),
		g_strcasecmp(require_client_certificate,"TRUE") == 0 ? TRUE: FALSE);
	gtk_table_attach (GTK_TABLE(table), requireCertificate, 0, 1, 12, 13, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
    gtk_widget_show_all(ret);
          
    if(gtk_dialog_run(GTK_DIALOG(tlsDialog)) == GTK_RESPONSE_ACCEPT) {        
            
    }    
    
    gtk_widget_destroy (GTK_WIDGET(tlsDialog));
}
