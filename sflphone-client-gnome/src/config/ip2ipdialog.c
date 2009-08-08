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

#include <zrtpadvanceddialog.h>
#include <sflphone_const.h>
#include <utils.h>

#include <gtk/gtk.h>

static void key_exchange_changed_cb(GtkWidget *widget, gpointer data)
{
    DEBUG("Key exchange changed");
    if (g_strcasecmp(gtk_combo_box_get_active_text(GTK_COMBO_BOX(widget)), (gchar *) "ZRTP") == 0) {
        gtk_widget_set_sensitive(GTK_WIDGET(data), TRUE);
    } else {
        gtk_widget_set_sensitive(GTK_WIDGET(data), FALSE);
        
    }
}

static void show_advanced_zrtp_options_cb(GtkWidget *widget UNUSED, gpointer data)
{
    DEBUG("Advanced options for ZRTP");
    show_advanced_zrtp_options((GHashTable *) data);
}

void show_ip2ip_dialog(GHashTable * properties)
{
    GtkDialog * ip2ipDialog;

    GtkWidget * frame;
    GtkWidget * table;
    GtkWidget * label;
    GtkWidget * enableHelloHash;
    GtkWidget * enableSASConfirm;
    GtkWidget * enableZrtpNotSuppOther;
    GtkWidget * displaySasOnce;
    GtkWidget * advancedOptions; 
    GtkWidget * keyExchangeCombo;
    
    gchar * curSasConfirm = "TRUE";
    gchar * curHelloEnabled = "TRUE";
    gchar * curZrtpNotSuppOther = "TRUE";
    gchar * curDisplaySasOnce = "FALSE";
    gchar * curSRTPEnabled = "FALSE";
    gchar * curKeyExchange = "0";
    gchar * description;
        
    if(properties != NULL) {
        curSRTPEnabled = g_hash_table_lookup(properties, ACCOUNT_ZRTP_HELLO_HASH);
        curKeyExchange = g_hash_table_lookup(properties, ACCOUNT_KEY_EXCHANGE);
        curHelloEnabled = g_hash_table_lookup(properties, ACCOUNT_ZRTP_HELLO_HASH);
        curSasConfirm = g_hash_table_lookup(properties, ACCOUNT_ZRTP_DISPLAY_SAS);
        curZrtpNotSuppOther = g_hash_table_lookup(properties, ACCOUNT_ZRTP_NOT_SUPP_WARNING);
        curDisplaySasOnce = g_hash_table_lookup(properties, ACCOUNT_DISPLAY_SAS_ONCE); 
    }
    
    ip2ipDialog = GTK_DIALOG(gtk_dialog_new_with_buttons (_("Direct peer to peer calls"),
                GTK_WINDOW(get_main_window()),
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_STOCK_HELP, 
                GTK_RESPONSE_HELP,
                GTK_STOCK_CANCEL,
                GTK_RESPONSE_CANCEL,
                GTK_STOCK_SAVE,
                GTK_RESPONSE_ACCEPT,
                NULL));
                
    gtk_window_set_policy( GTK_WINDOW(ip2ipDialog), FALSE, FALSE, FALSE );
    gtk_dialog_set_has_separator(ip2ipDialog, TRUE);
    gtk_container_set_border_width (GTK_CONTAINER(ip2ipDialog), 0);

	GtkWidget * vbox = gtk_vbox_new(FALSE, 10);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);

    gtk_box_pack_start(GTK_BOX(ip2ipDialog->vbox), vbox, FALSE, FALSE, 0);  

    description = g_markup_printf_escaped(_("This profile is used when you want to reach a remote peer\nby simply typing <b>sip:remotepeer</b> without having to go throught\nan external server. The settings here defined will also apply\nin case no account could be matched to the incoming or\noutgoing call."));
    label = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(label), description);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_FILL);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
               
     /* SRTP Section */
    gnome_main_section_new_with_table (_("Security"), &frame, &table, 1, 3);
	gtk_container_set_border_width (GTK_CONTAINER(table), 10);
	gtk_table_set_row_spacings (GTK_TABLE(table), 10);
    gtk_box_pack_start(GTK_BOX(vbox), frame, FALSE, FALSE, 0);

    label = gtk_label_new_with_mnemonic (_("SRTP key exchange"));
    keyExchangeCombo = gtk_combo_box_new_text();
    gtk_label_set_mnemonic_widget (GTK_LABEL (label), keyExchangeCombo);
    gtk_combo_box_append_text(GTK_COMBO_BOX(keyExchangeCombo), "ZRTP");
    //gtk_combo_box_append_text(GTK_COMBO_BOX(keyExchangeCombo), "SDES");
    gtk_combo_box_append_text(GTK_COMBO_BOX(keyExchangeCombo), _("Disabled"));      
    
    advancedOptions = gtk_button_new_with_label(_("Advanced options"));
    g_signal_connect(G_OBJECT(advancedOptions), "clicked", G_CALLBACK(show_advanced_zrtp_options_cb), properties);
    
    DEBUG("curSRTPenabled = %s\n", curSRTPEnabled);
    
    if (g_strcasecmp(curSRTPEnabled, "FALSE") == 0)
    {
        gtk_combo_box_set_active(GTK_COMBO_BOX(keyExchangeCombo), 1);
        gtk_widget_set_sensitive(GTK_WIDGET(advancedOptions), FALSE);
    } else {
        DEBUG("curKeyExchange %s \n", curKeyExchange);
        if (strcmp(curKeyExchange, "0") == 0) {
            gtk_combo_box_set_active(GTK_COMBO_BOX(keyExchangeCombo),0);
        } else {
            gtk_combo_box_set_active(GTK_COMBO_BOX(keyExchangeCombo), 1);
            gtk_widget_set_sensitive(GTK_WIDGET(advancedOptions), FALSE);
        }
    }
    
	g_signal_connect (G_OBJECT (GTK_COMBO_BOX(keyExchangeCombo)), "changed", G_CALLBACK (key_exchange_changed_cb), advancedOptions);
    
    gtk_table_attach_defaults(GTK_TABLE(table), label, 0, 1, 0, 1);
    gtk_table_attach_defaults(GTK_TABLE(table), keyExchangeCombo, 1, 2, 0, 1);    
    gtk_table_attach_defaults(GTK_TABLE(table), advancedOptions, 2, 3, 0, 1);

    gtk_widget_show_all(vbox);
        
    if(gtk_dialog_run(GTK_DIALOG(ip2ipDialog)) == GTK_RESPONSE_ACCEPT) {        
            gchar* keyExchange = (gchar *)gtk_combo_box_get_active_text(GTK_COMBO_BOX(keyExchangeCombo));
            DEBUG("Active text %s\n", keyExchange);
            if (g_strcmp0(keyExchange, "ZRTP") == 0) {
                g_hash_table_replace(properties, g_strdup(ACCOUNT_SRTP_ENABLED), g_strdup("TRUE"));
            } else {
                g_hash_table_replace(properties, g_strdup(ACCOUNT_SRTP_ENABLED), g_strdup("FALSE"));
            }              
    }    
    
    gtk_widget_destroy (GTK_WIDGET(ip2ipDialog));
}
