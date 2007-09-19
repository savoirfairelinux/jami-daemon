/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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
 
#include <config.h>
#include <mainwindow.h>
#include <accountlist.h>
#include <string.h>
#include <dbus.h>
#include <gtk/gtk.h>

/** Local variables */
account_t * currentAccount;

/**
 * Delete an account
 */
/*static void 
delete_account( GtkWidget *widget, gpointer   data )
{
  sflphone_remove_account(currentAccount);
}*/


void
show_account_window (account_t * a)
{
  
  GtkDialog * dialog;
  GtkWidget * table;
  GtkWidget * label;
  GtkWidget * entryID;
  GtkWidget * entryName;
  GtkWidget * entryProtocol;
  GtkWidget * entryEnabled;
  GtkWidget * entryRegister;
  GtkWidget * entryFullName;
  GtkWidget * entryUserPart;
  GtkWidget * entryHostPart;
  GtkWidget * entryUsername;
  GtkWidget * entryPassword;
  guint response;
  
  currentAccount = a;
  
  dialog = GTK_DIALOG(gtk_dialog_new_with_buttons ("Account settings",
                                        GTK_WINDOW(get_main_window()),
                                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_STOCK_CANCEL,
                                        GTK_RESPONSE_CANCEL,
                                        GTK_STOCK_SAVE,
                                        GTK_RESPONSE_ACCEPT,
                                        NULL));
                                        
  gtk_dialog_set_has_separator(dialog, TRUE);
  gtk_container_set_border_width (GTK_CONTAINER(dialog), 0);
  
  table = gtk_table_new ( 8, 2  , FALSE /* homogeneous */);
  gtk_table_set_row_spacings( GTK_TABLE(table), 10);
  gtk_table_set_col_spacings( GTK_TABLE(table), 10);
  
#ifdef DEBUG  
  label = gtk_label_new_with_mnemonic ("ID:");
  gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
  entryID = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryID);
  gtk_entry_set_text(GTK_ENTRY(entryID), a->accountID);
  gtk_widget_set_sensitive( GTK_WIDGET(entryID), FALSE);
  gtk_table_attach ( GTK_TABLE( table ), entryID, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
#endif 
  
  entryEnabled = gtk_check_button_new_with_mnemonic("_Enabled");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(entryEnabled), 
    strcmp(g_hash_table_lookup(currentAccount->properties, ACCOUNT_ENABLED),"TRUE") == 0 ? TRUE: FALSE); 
  gtk_table_attach ( GTK_TABLE( table ), entryEnabled, 0, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  entryRegister = gtk_check_button_new_with_mnemonic("_Register on startup ");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(entryRegister), 
    strcmp(g_hash_table_lookup(currentAccount->properties, ACCOUNT_REGISTER),"TRUE") == 0 ? TRUE: FALSE); 
  gtk_table_attach ( GTK_TABLE( table ), entryRegister, 0, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  label = gtk_label_new_with_mnemonic ("_Alias:");
  gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
  entryName = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryName);
  gtk_entry_set_text(GTK_ENTRY(entryName), g_hash_table_lookup(currentAccount->properties, ACCOUNT_ALIAS));
  gtk_table_attach ( GTK_TABLE( table ), entryName, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  label = gtk_label_new_with_mnemonic ("_Protocol:");
  gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
  entryProtocol = gtk_combo_box_new_text();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryProtocol);
  gtk_widget_set_sensitive( GTK_WIDGET(entryProtocol), FALSE); /* TODO When IAX is ok */
  gtk_combo_box_append_text(GTK_COMBO_BOX(entryProtocol), "SIP");
  gtk_combo_box_append_text(GTK_COMBO_BOX(entryProtocol), "IAX");
  if(strcmp(g_hash_table_lookup(a->properties, ACCOUNT_TYPE), "SIP") == 0)
  {
    gtk_combo_box_set_active(GTK_COMBO_BOX(entryProtocol),0);
  }
  else 
  {
    gtk_combo_box_set_active(GTK_COMBO_BOX(entryProtocol),1);
  }  
  gtk_table_attach ( GTK_TABLE( table ), entryProtocol, 1, 2, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  label = gtk_label_new_with_mnemonic ("_Full Name:");
  gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 5, 6, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
  entryFullName = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryFullName);
  gtk_entry_set_text(GTK_ENTRY(entryFullName), g_hash_table_lookup(currentAccount->properties, ACCOUNT_SIP_FULL_NAME));
  gtk_table_attach ( GTK_TABLE( table ), entryFullName, 1, 2, 5, 6, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  label = gtk_label_new_with_mnemonic ("_User part:");
  gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 6, 7, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
  entryUserPart = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryUserPart);
  gtk_entry_set_text(GTK_ENTRY(entryUserPart), g_hash_table_lookup(currentAccount->properties, ACCOUNT_SIP_USER_PART));
  gtk_table_attach ( GTK_TABLE( table ), entryUserPart, 1, 2, 6, 7, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  label = gtk_label_new_with_mnemonic ("_Host part:");
  gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 7, 8, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
  entryHostPart = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryHostPart);
  gtk_entry_set_text(GTK_ENTRY(entryHostPart), g_hash_table_lookup(currentAccount->properties, ACCOUNT_SIP_HOST_PART));
  gtk_table_attach ( GTK_TABLE( table ), entryHostPart, 1, 2, 7, 8, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  label = gtk_label_new_with_mnemonic ("U_sername:");
  gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 8, 9, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
  entryUsername = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryUsername);
  gtk_entry_set_text(GTK_ENTRY(entryUsername), g_hash_table_lookup(currentAccount->properties, ACCOUNT_SIP_AUTH_NAME));
  gtk_table_attach ( GTK_TABLE( table ), entryUsername, 1, 2, 8, 9, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  label = gtk_label_new_with_mnemonic ("_Password:");
  gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 9, 10, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
  entryPassword = gtk_entry_new();
  gtk_entry_set_visibility(GTK_ENTRY(entryPassword), FALSE);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), entryPassword);
  gtk_entry_set_text(GTK_ENTRY(entryPassword), g_hash_table_lookup(currentAccount->properties, ACCOUNT_SIP_PASSWORD));
  gtk_table_attach ( GTK_TABLE( table ), entryPassword, 1, 2, 9, 10, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  
  
  gtk_box_pack_start (GTK_BOX (dialog->vbox), table, TRUE, TRUE, 0);
  gtk_container_set_border_width (GTK_CONTAINER(table), 10);
  
  gtk_widget_show_all(table);
  
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if(response == GTK_RESPONSE_ACCEPT)
  { 
    g_hash_table_replace(currentAccount->properties, 
      g_strdup(ACCOUNT_ENABLED), 
      g_strdup(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(entryEnabled)) ? "TRUE": "FALSE"));
    g_hash_table_replace(currentAccount->properties, 
      g_strdup(ACCOUNT_REGISTER), 
      g_strdup(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(entryRegister)) ? "TRUE": "FALSE"));
    /* TODO Add SIP/IAX when IAX is ok */  
    g_hash_table_replace(currentAccount->properties, 
      g_strdup(ACCOUNT_ALIAS), 
      g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(entryName))));
    g_hash_table_replace(currentAccount->properties, 
      g_strdup(ACCOUNT_SIP_FULL_NAME), 
      g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(entryFullName))));
    g_hash_table_replace(currentAccount->properties, 
      g_strdup(ACCOUNT_SIP_USER_PART), 
      g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(entryUserPart))));
    g_hash_table_replace(currentAccount->properties, 
      g_strdup(ACCOUNT_SIP_HOST_PART), 
      g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(entryHostPart))));
    g_hash_table_replace(currentAccount->properties, 
      g_strdup(ACCOUNT_SIP_AUTH_NAME), 
      g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(entryUsername))));
    g_hash_table_replace(currentAccount->properties, 
      g_strdup(ACCOUNT_SIP_PASSWORD), 
      g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(entryPassword))));
      
    dbus_set_account_details(currentAccount);
  }
  gtk_widget_destroy (GTK_WIDGET(dialog));
  
  
}

