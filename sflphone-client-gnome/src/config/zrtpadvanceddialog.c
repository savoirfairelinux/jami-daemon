/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include <zrtpadvanceddialog.h>
#include <sflphone_const.h>
#include <utils.h>

#include <gtk/gtk.h>

void show_advanced_zrtp_options(GHashTable * properties)
{
    GtkDialog * securityDialog;

    GtkWidget * zrtpFrame;
    GtkWidget * tableZrtp;
    GtkWidget * enableHelloHash;
    GtkWidget * enableSASConfirm;
    GtkWidget * enableZrtpNotSuppOther;
    GtkWidget * displaySasOnce;
    
    gchar * curSasConfirm = "true";
    gchar * curHelloEnabled = "true";
    gchar * curZrtpNotSuppOther = "true";
    gchar * curDisplaySasOnce = "false";
    
    if(properties != NULL) {
        curHelloEnabled = g_hash_table_lookup(properties, ACCOUNT_ZRTP_HELLO_HASH);
        curSasConfirm = g_hash_table_lookup(properties, ACCOUNT_ZRTP_DISPLAY_SAS);
        curZrtpNotSuppOther = g_hash_table_lookup(properties, ACCOUNT_ZRTP_NOT_SUPP_WARNING);
        curDisplaySasOnce = g_hash_table_lookup(properties, ACCOUNT_DISPLAY_SAS_ONCE); 
    }
    
    securityDialog = GTK_DIALOG	(gtk_dialog_new_with_buttons (	_("ZRTP Options"),
			    GTK_WINDOW (get_main_window()),
																GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
																GTK_STOCK_CANCEL,
																GTK_RESPONSE_CANCEL,
																GTK_STOCK_SAVE,
																GTK_RESPONSE_ACCEPT,
																NULL)
								);
    gtk_window_set_policy( GTK_WINDOW(securityDialog), FALSE, FALSE, FALSE );
    gtk_dialog_set_has_separator(securityDialog, TRUE);
    gtk_container_set_border_width (GTK_CONTAINER(securityDialog), 0);

    
    tableZrtp = gtk_table_new (4, 2  , FALSE/* homogeneous */);  
    gtk_table_set_row_spacings( GTK_TABLE(tableZrtp), 10);
    gtk_table_set_col_spacings( GTK_TABLE(tableZrtp), 10); 
    gtk_box_pack_start(GTK_BOX(securityDialog->vbox), tableZrtp, FALSE, FALSE, 0);  
    gtk_widget_show(tableZrtp);
    
    enableHelloHash = gtk_check_button_new_with_mnemonic(_("Send Hello Hash in S_DP"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableHelloHash),
            g_strcasecmp(curHelloEnabled,"true") == 0 ? TRUE: FALSE);
    gtk_table_attach ( GTK_TABLE(tableZrtp), enableHelloHash, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_set_sensitive( GTK_WIDGET( enableHelloHash ) , TRUE );
        
    enableSASConfirm = gtk_check_button_new_with_mnemonic(_("Ask User to Confirm SAS"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableSASConfirm),
            g_strcasecmp(curSasConfirm,"true") == 0 ? TRUE: FALSE);
    gtk_table_attach ( GTK_TABLE(tableZrtp), enableSASConfirm, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_set_sensitive( GTK_WIDGET( enableSASConfirm ) , TRUE ); 
  
    enableZrtpNotSuppOther = gtk_check_button_new_with_mnemonic(_("_Warn if ZRTP not supported"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableZrtpNotSuppOther),
            g_strcasecmp(curZrtpNotSuppOther,"true") == 0 ? TRUE: FALSE);
    gtk_table_attach ( GTK_TABLE(tableZrtp), enableZrtpNotSuppOther, 0, 1, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_set_sensitive( GTK_WIDGET( enableZrtpNotSuppOther ) , TRUE );
  
    displaySasOnce = gtk_check_button_new_with_mnemonic(_("Display SAS once for hold events"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(displaySasOnce),
            g_strcasecmp(curDisplaySasOnce,"true") == 0 ? TRUE: FALSE);
    gtk_table_attach ( GTK_TABLE(tableZrtp), displaySasOnce, 0, 1, 5, 6, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_set_sensitive( GTK_WIDGET( displaySasOnce ) , TRUE );
    
    gtk_widget_show_all(tableZrtp);

    gtk_container_set_border_width (GTK_CONTAINER(tableZrtp), 10);
        
    if(gtk_dialog_run(GTK_DIALOG(securityDialog)) == GTK_RESPONSE_ACCEPT) {        
        g_hash_table_replace(properties,
                g_strdup(ACCOUNT_ZRTP_DISPLAY_SAS),
                g_strdup(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enableSASConfirm)) ? "true": "false"));   
                
         g_hash_table_replace(properties,
                g_strdup(ACCOUNT_DISPLAY_SAS_ONCE),
                g_strdup(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(displaySasOnce)) ? "true": "false")); 
                
        g_hash_table_replace(properties,
                g_strdup(ACCOUNT_ZRTP_HELLO_HASH),
                g_strdup(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enableHelloHash)) ? "true": "false"));
                
        g_hash_table_replace(properties,
                g_strdup(ACCOUNT_ZRTP_NOT_SUPP_WARNING),
                g_strdup(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enableZrtpNotSuppOther)) ? "true": "false"));                
    }    
    
    gtk_widget_destroy (GTK_WIDGET(securityDialog));
}


void show_advanced_sdes_options(GHashTable * properties) {

    GtkDialog * securityDialog;

    GtkWidget * sdesTable;
    GtkWidget * enableRtpFallback;
    gchar * rtpFallback = "false";
    
    if(properties != NULL) {
        rtpFallback = g_hash_table_lookup(properties, ACCOUNT_SRTP_RTP_FALLBACK);
    }

    securityDialog = GTK_DIALOG	(gtk_dialog_new_with_buttons (	_("SDES Options"),

			       	GTK_WINDOW (get_main_window()),							
				GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,

				GTK_STOCK_CANCEL,

			        GTK_RESPONSE_CANCEL,

				GTK_STOCK_SAVE,

				GTK_RESPONSE_ACCEPT,	       
				
				NULL));

    gtk_window_set_policy( GTK_WINDOW(securityDialog), FALSE, FALSE, FALSE );
    gtk_dialog_set_has_separator(securityDialog, TRUE);
    gtk_container_set_border_width (GTK_CONTAINER(securityDialog), 0);

    sdesTable = gtk_table_new (1, 2  , FALSE/* homogeneous */);  
    gtk_table_set_row_spacings( GTK_TABLE(sdesTable), 10);
    gtk_table_set_col_spacings( GTK_TABLE(sdesTable), 10); 
    gtk_box_pack_start(GTK_BOX(securityDialog->vbox), sdesTable, FALSE, FALSE, 0);  
    gtk_widget_show(sdesTable);

    enableRtpFallback = gtk_check_button_new_with_mnemonic(_("Fallback on RTP on SDES failure"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableRtpFallback),
				 g_strcasecmp(rtpFallback,"true") == 0 ? TRUE: FALSE);
    gtk_table_attach ( GTK_TABLE(sdesTable), enableRtpFallback, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_widget_set_sensitive( GTK_WIDGET( enableRtpFallback ) , TRUE );


    gtk_widget_show_all(sdesTable);

    gtk_container_set_border_width (GTK_CONTAINER(sdesTable), 10);
        
    if(gtk_dialog_run(GTK_DIALOG(securityDialog)) == GTK_RESPONSE_ACCEPT) {        
        g_hash_table_replace(properties,
                g_strdup(ACCOUNT_SRTP_RTP_FALLBACK),
	        g_strdup(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(enableRtpFallback)) ? "true": "false"));                
    }    
    
    gtk_widget_destroy (GTK_WIDGET(securityDialog));
}
