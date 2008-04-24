/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
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

#include <druid.h>

static struct _wizard *wiz;
static int account_type;
account_t* current;

void
build_account_configuration( int type )
{
  // table
  wiz->table = gtk_table_new ( 8, 4  ,  FALSE/* homogeneous */);
  gtk_table_set_row_spacings( GTK_TABLE(wiz->table), 10);
  gtk_table_set_col_spacings( GTK_TABLE(wiz->table), 10);
  if( type == _SIP )
    gtk_box_pack_start(GTK_BOX(GNOME_DRUID_PAGE_STANDARD(wiz->sip_account)->vbox),wiz->table, TRUE, TRUE, 2);
  else
    gtk_box_pack_start(GTK_BOX(GNOME_DRUID_PAGE_STANDARD(wiz->iax_account)->vbox),wiz->table, TRUE, TRUE, 2);

  // alias field
  wiz->label = gtk_label_new_with_mnemonic ("_Alias");
  gtk_table_attach ( GTK_TABLE( wiz->table ), wiz->label, 0, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (wiz->label), 0, 0.5);
  wiz->alias = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (wiz->label), wiz->alias);
  gtk_table_attach ( GTK_TABLE( wiz->table ), wiz->alias, 2, 4, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

  // name field
  wiz->label = gtk_label_new_with_mnemonic ("_Name");
  gtk_table_attach ( GTK_TABLE( wiz->table ), wiz->label, 0, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (wiz->label), 0, 0.5);
  wiz->name = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (wiz->label), wiz->name);
  gtk_table_attach ( GTK_TABLE( wiz->table ), wiz->name, 2, 4, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

  if( type == _SIP )
  {
    // user part field
    wiz->label = gtk_label_new_with_mnemonic ("_Userpart");
    gtk_table_attach ( GTK_TABLE( wiz->table ), wiz->label, 0, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    gtk_misc_set_alignment(GTK_MISC (wiz->label), 0, 0.5);
    wiz->userpart = gtk_entry_new();
    gtk_label_set_mnemonic_widget (GTK_LABEL (wiz->label), wiz->userpart);
    gtk_table_attach ( GTK_TABLE( wiz->table ), wiz->userpart, 2, 4, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  }

  // server field
  wiz->label = gtk_label_new_with_mnemonic ("_Server");
  gtk_table_attach ( GTK_TABLE( wiz->table ), wiz->label, 0, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (wiz->label), 0, 0.5);
  wiz->server = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (wiz->label), wiz->server);
  gtk_table_attach ( GTK_TABLE( wiz->table ), wiz->server, 2, 4, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  // username field
  wiz->label = gtk_label_new_with_mnemonic ("_Username");
  gtk_table_attach ( GTK_TABLE( wiz->table ), wiz->label, 0, 2, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (wiz->label), 0, 0.5);
  wiz->username = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (wiz->label), wiz->username);
  gtk_table_attach ( GTK_TABLE( wiz->table ), wiz->username, 2, 4, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  // password field
  wiz->label = gtk_label_new_with_mnemonic ("_Password");
  gtk_table_attach ( GTK_TABLE( wiz->table ), wiz->label, 0, 2, 5, 6, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (wiz->label), 0, 0.5);
  wiz->password = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (wiz->label), wiz->password);
  gtk_table_attach ( GTK_TABLE( wiz->table ), wiz->password, 2, 4, 5, 6, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

  // mailbox field
  wiz->label = gtk_label_new_with_mnemonic ("_Mailbox");
  gtk_table_attach ( GTK_TABLE( wiz->table ), wiz->label, 0, 2, 6, 7, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (wiz->label), 0, 0.5);
  wiz->mailbox = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (wiz->label), wiz->mailbox);
  gtk_table_attach ( GTK_TABLE( wiz->table ), wiz->mailbox, 2, 4, 6, 7, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
}

void
update_account_parameters( int type )
{ 
  g_hash_table_replace(current->properties, g_strdup(ACCOUNT_ALIAS), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->alias))));

  if( type == _SIP ){
    g_hash_table_replace(current->properties, g_strdup(ACCOUNT_SIP_FULL_NAME), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->name))));
    g_hash_table_replace(current->properties, g_strdup(ACCOUNT_SIP_USER_PART), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->userpart))));
    g_hash_table_replace(current->properties, g_strdup(ACCOUNT_SIP_HOST_PART), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->server))));
    g_hash_table_replace(current->properties, g_strdup(ACCOUNT_SIP_PASSWORD), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->password))));
    g_hash_table_replace(current->properties, g_strdup(ACCOUNT_SIP_AUTH_NAME), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->username))));
    g_hash_table_replace(current->properties, g_strdup(ACCOUNT_SIP_STUN_ENABLED), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->enable))));
    g_hash_table_replace(current->properties, g_strdup(ACCOUNT_SIP_STUN_SERVER), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->addr))));
    g_hash_table_replace(current->properties, g_strdup(ACCOUNT_MAILBOX), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->mailbox))));
  
  }
  else if( type == _IAX ){
    g_hash_table_replace(current->properties, g_strdup(ACCOUNT_IAX_FULL_NAME), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->name))));
    g_hash_table_replace(current->properties, g_strdup(ACCOUNT_IAX_USER), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->username))));
    g_hash_table_replace(current->properties, g_strdup(ACCOUNT_IAX_HOST), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->server))));
    g_hash_table_replace(current->properties, g_strdup(ACCOUNT_IAX_PASS), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->password))));
    g_hash_table_replace(current->properties, g_strdup(ACCOUNT_MAILBOX), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->mailbox))));
  }
  //dbus_add_account(current);
}

void
build_nat_window( void )
{
  // table
  wiz->table = gtk_table_new ( 2, 4  ,  FALSE/* homogeneous */);
  gtk_table_set_row_spacings( GTK_TABLE(wiz->table), 10);
  gtk_table_set_col_spacings( GTK_TABLE(wiz->table), 10);
  gtk_box_pack_start(GTK_BOX(GNOME_DRUID_PAGE_STANDARD(wiz->nat)->vbox),wiz->table, TRUE, TRUE, 2);

  // enable
  wiz->enable = gtk_check_button_new_with_mnemonic("_Enabled");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wiz->enable), FALSE); 
  gtk_table_attach ( GTK_TABLE( wiz->table ), wiz->enable, 0, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_set_sensitive( GTK_WIDGET( wiz->enable ) , TRUE );
  g_signal_connect( G_OBJECT( GTK_TOGGLE_BUTTON(wiz->enable)) , "toggled" , G_CALLBACK( enable_stun ), NULL);

  // server address
  wiz->label = gtk_label_new_with_mnemonic ("_Server address");
  gtk_table_attach ( GTK_TABLE( wiz->table ), wiz->label, 0, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (wiz->label), 0, 0.5);
  wiz->addr = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (wiz->label), wiz->addr);
  gtk_table_attach ( GTK_TABLE( wiz->table ), wiz->addr, 2, 4, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_set_sensitive( GTK_WIDGET( wiz->addr ), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wiz->enable)));

}

void
build_registration_test( void )
{
}

void
set_account_type( GtkWidget* widget , gpointer data )
{
  
  if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget )) ){
    account_type = _SIP;
    g_hash_table_replace(current->properties, g_strdup(ACCOUNT_TYPE), g_strdup("SIP"));
  }else{ 
    account_type = _IAX ;
    g_hash_table_replace(current->properties, g_strdup(ACCOUNT_TYPE), g_strdup("IAX"));
  }
}

void
enable_stun( GtkWidget* widget )
{
  gtk_widget_set_sensitive( GTK_WIDGET( wiz->addr ), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}

void
goto_right_account( void )
{
  if( account_type == _SIP )
    gnome_druid_set_page( GNOME_DRUID( wiz->druid ) , GNOME_DRUID_PAGE( wiz->sip_account));
  else
    gnome_druid_set_page( GNOME_DRUID( wiz->druid ) , GNOME_DRUID_PAGE( wiz->iax_account));
}

void
goto_accounts_page( void )
{
    gnome_druid_set_page( GNOME_DRUID( wiz->druid ) , GNOME_DRUID_PAGE( wiz->account_type));
}

void
goto_nat_page( void )
{
    update_account_parameters( _SIP );
    gnome_druid_set_page( GNOME_DRUID( wiz->druid ) , GNOME_DRUID_PAGE( wiz->nat));
}

void
goto_end_page( void )
{
  dbus_add_account( current );
  gnome_druid_set_page( GNOME_DRUID( wiz->druid ) , GNOME_DRUID_PAGE( wiz->page_end));
}

void
goto_sip_account_page( void )
{
    gnome_druid_set_page( GNOME_DRUID( wiz->druid ) , GNOME_DRUID_PAGE( wiz->sip_account));
}

void
quit_wizard( void )
{
  return;
}

void
build_wizard( void )
{
  wiz = ( struct _wizard* )g_malloc( sizeof( struct _wizard));
  current = g_new0(account_t, 1);
  current->properties = g_hash_table_new(NULL, g_str_equal);

  wiz->logo = gdk_pixbuf_new_from_file(ICON_DIR "/sflphone.png", NULL);

  wiz->druid = gnome_druid_new_with_window( "SFLphone" , NULL , TRUE , &wiz->window );
  wiz->first_page = gnome_druid_page_edge_new( GNOME_EDGE_START );
  gnome_druid_append_page( GNOME_DRUID( wiz->druid ) , GNOME_DRUID_PAGE( wiz->first_page ) );
  gnome_druid_page_edge_set_title( GNOME_DRUID_PAGE_EDGE( wiz->first_page ), "SFLphone 0.8-5");
  gnome_druid_page_edge_set_text(  GNOME_DRUID_PAGE_EDGE( wiz->first_page ),
				    "  Welcome. \n\n\n  This wizard will help you configure an account.\n\n\n   Answer the call ! " );
  
  /** Page 1 */
  wiz->account_type = gnome_druid_page_standard_new();
  gnome_druid_page_standard_set_title( GNOME_DRUID_PAGE_STANDARD( wiz->account_type), "Select a VoIP protocol");
  gnome_druid_page_standard_set_logo( GNOME_DRUID_PAGE_STANDARD( wiz->account_type) , wiz->logo );
  gnome_druid_append_page( GNOME_DRUID( wiz->druid ) , GNOME_DRUID_PAGE( wiz->account_type ));
  wiz->protocols = gtk_vbox_new(FALSE , 10);
  gtk_box_pack_start(GTK_BOX(GNOME_DRUID_PAGE_STANDARD(wiz->account_type)->vbox),wiz->protocols, TRUE, TRUE, 2);
  wiz->sip = gtk_radio_button_new_with_label(NULL,"SIP (Session Initiation Protocol)");
  gtk_box_pack_start( GTK_BOX(wiz->protocols) , wiz->sip , TRUE, TRUE, 0);
  wiz->iax = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(wiz->sip), "IAX2 (InterAsterix Exchange)");
  gtk_box_pack_start( GTK_BOX(wiz->protocols) , wiz->iax , TRUE, TRUE, 0);
  g_signal_connect(G_OBJECT( wiz->sip ) , "clicked" , G_CALLBACK( set_account_type ) , NULL);

  /** Page 2 */
  wiz->sip_account = gnome_druid_page_standard_new();
  gnome_druid_page_standard_set_title( GNOME_DRUID_PAGE_STANDARD( wiz->sip_account), "SIP account configuration");
  gnome_druid_page_standard_set_logo( GNOME_DRUID_PAGE_STANDARD( wiz->sip_account) , wiz->logo );
  gnome_druid_append_page( GNOME_DRUID( wiz->druid ) , GNOME_DRUID_PAGE( wiz->sip_account ));
  build_account_configuration( _SIP );

  /** Page 3 */
  wiz->iax_account = gnome_druid_page_standard_new();
  gnome_druid_page_standard_set_title( GNOME_DRUID_PAGE_STANDARD( wiz->iax_account), "IAX2 account configuration");
  gnome_druid_page_standard_set_logo( GNOME_DRUID_PAGE_STANDARD( wiz->iax_account) , wiz->logo );
  gnome_druid_append_page( GNOME_DRUID( wiz->druid ) , GNOME_DRUID_PAGE( wiz->iax_account ));
  build_account_configuration( _IAX );

  /** Page 4 */
  wiz->nat = gnome_druid_page_standard_new();
  gnome_druid_page_standard_set_title( GNOME_DRUID_PAGE_STANDARD( wiz->nat), "Nat detection");
  gnome_druid_page_standard_set_logo( GNOME_DRUID_PAGE_STANDARD( wiz->nat) , wiz->logo );
  gnome_druid_append_page( GNOME_DRUID( wiz->druid ) , GNOME_DRUID_PAGE( wiz->nat ));
  build_nat_window();

  /** Page 5 */
  wiz->page_end = gnome_druid_page_edge_new( GNOME_EDGE_FINISH );
  gnome_druid_page_edge_set_title( GNOME_DRUID_PAGE_EDGE( wiz->page_end), "Account Registration");
  gnome_druid_page_edge_set_logo( GNOME_DRUID_PAGE_EDGE( wiz->page_end) , wiz->logo );
  gnome_druid_append_page( GNOME_DRUID( wiz->druid ) , GNOME_DRUID_PAGE( wiz->page_end ));
  gnome_druid_page_edge_set_text(  GNOME_DRUID_PAGE_EDGE( wiz->page_end ),
				    "  Congratulations! \n\n You have been successfully registered " );

  /** Events */
  g_signal_connect( G_OBJECT( wiz->account_type ) , "next" , G_CALLBACK( goto_right_account ) , NULL );
  g_signal_connect( G_OBJECT( wiz->iax_account ) , "back" , G_CALLBACK( goto_accounts_page ), NULL );
  g_signal_connect( G_OBJECT( wiz->sip_account ) , "next" , G_CALLBACK( goto_nat_page ), NULL );
  g_signal_connect( G_OBJECT( wiz->iax_account ) , "next" , G_CALLBACK( goto_end_page ), NULL );
  g_signal_connect( G_OBJECT( wiz->nat ) , "back" , G_CALLBACK( goto_sip_account_page ), NULL );
  g_signal_connect( G_OBJECT( wiz->page_end ) , "finish" , G_CALLBACK( quit_wizard ), NULL );
  

  gtk_widget_show_all(wiz->window);
}


