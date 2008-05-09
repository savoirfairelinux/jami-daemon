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

#include <assistant.h>

struct _wizard *wiz;
static int account_type;
account_t* current;

  void
set_account_type( GtkWidget* widget , gpointer data )
{

  if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( widget )) ){
    account_type = _SIP;
  }else{ 
    account_type = _IAX ;
  }
}

static void close_callback( void )
{
  gtk_widget_destroy(wiz->assistant);
}

static void cancel_callback( void )
{
  gtk_widget_destroy(wiz->assistant);
}

  static void
sip_apply_callback( void )
{
  if( account_type == _SIP )
  {
    g_print("SIP APPLY CALLBACK\n");
    g_hash_table_insert(current->properties, g_strdup(ACCOUNT_ALIAS), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->sip_alias))));
    g_hash_table_insert(current->properties, g_strdup(ACCOUNT_ENABLED), g_strdup("TRUE"));
    g_hash_table_insert(current->properties, g_strdup(ACCOUNT_MAILBOX), g_strdup("888"));
    g_hash_table_insert(current->properties, g_strdup(ACCOUNT_TYPE), g_strdup("SIP"));
    g_hash_table_insert(current->properties, g_strdup(ACCOUNT_SIP_HOST), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->sip_server))));
    g_hash_table_insert(current->properties, g_strdup(ACCOUNT_SIP_PASSWORD), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->sip_password))));
    g_hash_table_insert(current->properties, g_strdup(ACCOUNT_SIP_USER), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->sip_username))));
    g_hash_table_insert(current->properties, g_strdup(ACCOUNT_SIP_STUN_ENABLED), g_strdup((gchar *)gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wiz->enable))? "TRUE":"FALSE"));
    g_hash_table_insert(current->properties, g_strdup(ACCOUNT_SIP_STUN_SERVER), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->addr))));

    dbus_add_account( current );
    account_list_set_current_id( current->accountID );
    g_print( "ACCOUNT ID = %s\n" , current->accountID );
  }
}
  static void 
iax_apply_callback( void ) 
{
  if( account_type == _IAX)
  {
    g_print("IAX APPLY CALLBACK\n");
    g_hash_table_insert(current->properties, g_strdup(ACCOUNT_ALIAS), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->iax_alias))));
    g_hash_table_insert(current->properties, g_strdup(ACCOUNT_ENABLED), g_strdup("TRUE"));
    g_hash_table_insert(current->properties, g_strdup(ACCOUNT_MAILBOX), g_strdup("888"));
    g_hash_table_insert(current->properties, g_strdup(ACCOUNT_TYPE), g_strdup("IAX"));
    g_hash_table_insert(current->properties, g_strdup(ACCOUNT_IAX_USER), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->iax_username))));
    g_hash_table_insert(current->properties, g_strdup(ACCOUNT_IAX_HOST), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->iax_server))));
    g_hash_table_insert(current->properties, g_strdup(ACCOUNT_IAX_PASSWORD), g_strdup((gchar *)gtk_entry_get_text(GTK_ENTRY(wiz->iax_password))));

    dbus_add_account( current );
    account_list_set_current_id( current->accountID );
    g_print( "ACCOUNT ID = %s\n" , current->accountID );
  }
}

  void
enable_stun( GtkWidget* widget )
{
  gtk_widget_set_sensitive( GTK_WIDGET( wiz->addr ), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget)));
}

  void
build_wizard( void )
{
  wiz = ( struct _wizard* )g_malloc( sizeof( struct _wizard));
  current = g_new0(account_t, 1);
  current->properties = g_hash_table_new(NULL, g_str_equal);
  //current ->accountID = "test";

  wiz->assistant = gtk_assistant_new( );
  gtk_window_set_title( GTK_WINDOW(wiz->assistant), _("SFLphone account configuration wizard") );
  gtk_window_set_position(GTK_WINDOW(wiz->assistant), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(wiz->assistant), 200 , 200);
  gtk_assistant_set_forward_page_func( GTK_ASSISTANT( wiz->assistant ), (GtkAssistantPageFunc) forward_page_func , NULL , NULL );

  build_intro();
  build_select_account();
  build_sip_account_configuration();
  build_nat_settings();
  build_iax_account_configuration();
  build_registration_error();
  build_summary();

  g_signal_connect(G_OBJECT(wiz->assistant), "close" , G_CALLBACK(close_callback), NULL);
  g_signal_connect(G_OBJECT(wiz->assistant), "cancel" , G_CALLBACK(cancel_callback), NULL);

  gtk_widget_show_all(wiz->assistant);

  gtk_assistant_update_buttons_state(GTK_ASSISTANT(wiz->assistant));

}

  GtkWidget*
build_intro()
{
  GtkWidget *label;

  wiz->intro = create_vbox( GTK_ASSISTANT_PAGE_INTRO  , _("SFLphone 0.8") , _("Welcome to SFLphone!"));

  label = gtk_label_new(_("This installation wizard will help you configure an account.")) ;
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_widget_set_size_request(GTK_WIDGET(label), 380, -1);
  gtk_box_pack_start(GTK_BOX(wiz->intro), label, FALSE, TRUE, 0);

  return wiz->intro;
}

  GtkWidget*
build_select_account()
{
  GtkWidget* sip;
  GtkWidget* iax;

  wiz->protocols = create_vbox( GTK_ASSISTANT_PAGE_CONTENT , _("VoIP Protocols") , _("Select an account type:"));

  sip = gtk_radio_button_new_with_label(NULL,"SIP (Session Initiation Protocol)");
  gtk_box_pack_start( GTK_BOX(wiz->protocols) , sip , TRUE, TRUE, 0);
  iax = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(sip), "IAX2 (InterAsterix Exchange)");
  gtk_box_pack_start( GTK_BOX(wiz->protocols) , iax , TRUE, TRUE, 0);

  g_signal_connect(G_OBJECT( sip ) , "clicked" , G_CALLBACK( set_account_type ) , NULL );

  return wiz->protocols;
}


  GtkWidget*
build_sip_account_configuration( void )
{
  GtkWidget* table;
  GtkWidget* label;

  wiz->sip_account = create_vbox( GTK_ASSISTANT_PAGE_CONTENT , _("SIP account configuration") , _("Please fill the following information:")); 
  // table
  table = gtk_table_new ( 4, 2  ,  FALSE/* homogeneous */);
  gtk_table_set_row_spacings( GTK_TABLE(table), 10);
  gtk_table_set_col_spacings( GTK_TABLE(table), 10);
  gtk_box_pack_start( GTK_BOX(wiz->sip_account) , table , TRUE, TRUE, 0);

  // alias field
  label = gtk_label_new_with_mnemonic (_("_Alias"));
  gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
  wiz->sip_alias = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), wiz->sip_alias);
  gtk_table_attach ( GTK_TABLE( table ), wiz->sip_alias, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

  // server field
  label = gtk_label_new_with_mnemonic (_("_Host name"));
  gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
  wiz->sip_server = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), wiz->sip_server);
  gtk_table_attach ( GTK_TABLE( table ), wiz->sip_server, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

  // username field
  label = gtk_label_new_with_mnemonic (_("_User name"));
  gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
  wiz->sip_username = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), wiz->sip_username);
  gtk_table_attach ( GTK_TABLE( table ), wiz->sip_username, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

  // password field
  label = gtk_label_new_with_mnemonic (_("_Password"));
  gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
  wiz->sip_password = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), wiz->sip_password);
  gtk_entry_set_visibility(GTK_ENTRY(wiz->sip_password), FALSE);
  gtk_table_attach ( GTK_TABLE( table ), wiz->sip_password, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

  return wiz->sip_account;
}

  GtkWidget*
build_iax_account_configuration( void )
{
  GtkWidget* label;
  GtkWidget*  table;

  wiz->iax_account = create_vbox( GTK_ASSISTANT_PAGE_CONFIRM , _("IAX2 account configuration") , _("Please fill the following information:")); 

  table = gtk_table_new ( 4, 2  ,  FALSE/* homogeneous */);
  gtk_table_set_row_spacings( GTK_TABLE(table), 10);
  gtk_table_set_col_spacings( GTK_TABLE(table), 10);
  gtk_box_pack_start( GTK_BOX(wiz->iax_account) , table , TRUE, TRUE, 0);

  // alias field
  label = gtk_label_new_with_mnemonic (_("_Alias"));
  gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
  wiz->iax_alias = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), wiz->iax_alias);
  gtk_table_attach ( GTK_TABLE( table ), wiz->iax_alias, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

  // server field
  label = gtk_label_new_with_mnemonic (_("_Host name"));
  gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
  wiz->iax_server = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), wiz->iax_server);
  gtk_table_attach ( GTK_TABLE( table ), wiz->iax_server, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

  // username field
  label = gtk_label_new_with_mnemonic (_("_User name"));
  gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
  wiz->iax_username = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), wiz->iax_username);
  gtk_table_attach ( GTK_TABLE( table ), wiz->iax_username, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

  // password field
  label = gtk_label_new_with_mnemonic (_("_Password"));
  gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
  wiz->iax_password = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), wiz->iax_password);
  gtk_entry_set_visibility(GTK_ENTRY(wiz->iax_password), FALSE);
  gtk_table_attach ( GTK_TABLE( table ), wiz->iax_password, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);


  current -> state = ACCOUNT_STATE_UNREGISTERED;

  g_signal_connect( G_OBJECT( wiz->assistant ) , "apply" , G_CALLBACK( iax_apply_callback ), NULL);

  return wiz->iax_account;
}

  GtkWidget*
build_nat_settings( void )
{
  GtkWidget* label;
  GtkWidget* table;

  wiz->nat = create_vbox( GTK_ASSISTANT_PAGE_CONFIRM , _("Network Address Translation") , _("You should probably enable this if you are behind a firewall.")); 

  // table
  table = gtk_table_new ( 2, 2  ,  FALSE/* homogeneous */);
  gtk_table_set_row_spacings( GTK_TABLE(table), 10);
  gtk_table_set_col_spacings( GTK_TABLE(table), 10);
  gtk_box_pack_start( GTK_BOX(wiz->nat), table , TRUE, TRUE, 0);

  // enable
  wiz->enable = gtk_check_button_new_with_mnemonic(_("E_nable STUN"));
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(wiz->enable), FALSE); 
  gtk_table_attach ( GTK_TABLE( table ), wiz->enable, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_set_sensitive( GTK_WIDGET( wiz->enable ) , TRUE );
  g_signal_connect( G_OBJECT( GTK_TOGGLE_BUTTON(wiz->enable)) , "toggled" , G_CALLBACK( enable_stun ), NULL);

  // server address
  label = gtk_label_new_with_mnemonic (_("_STUN server"));
  gtk_table_attach ( GTK_TABLE( table ), label, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_misc_set_alignment(GTK_MISC (label), 0, 0.5);
  wiz->addr = gtk_entry_new();
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), wiz->addr);
  gtk_table_attach ( GTK_TABLE( table ), wiz->addr, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_set_sensitive( GTK_WIDGET( wiz->addr ), gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(wiz->enable)));

  g_signal_connect( G_OBJECT( wiz->assistant ) , "apply" , G_CALLBACK( sip_apply_callback ), NULL);

  return wiz->nat;
}

  GtkWidget*
build_summary()
{
  GtkWidget *label;
  wiz->summary = create_vbox( GTK_ASSISTANT_PAGE_SUMMARY  , _("Account Registration") , _("Congratulations!"));

  label = gtk_label_new(_("This assistant is now finished.\n\n You can at any time check your registration state or modify your accounts parameters in the Options/Accounts window.")) ;
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_widget_set_size_request(GTK_WIDGET(label), 380, -1);
  gtk_box_pack_start(GTK_BOX(wiz->summary), label, FALSE, TRUE, 0);

  return wiz->summary;
}

  GtkWidget*
build_registration_error()
{
  GtkWidget *label;
  wiz->reg_failed = create_vbox( GTK_ASSISTANT_PAGE_SUMMARY  , "Account Registration" , "Registration error");

  label = gtk_label_new(" Please correct the information.") ;
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_widget_set_size_request(GTK_WIDGET(label), 380, -1);
  gtk_box_pack_start(GTK_BOX(wiz->reg_failed), label, FALSE, TRUE, 0);

  return wiz->reg_failed;

}

  static gint 
forward_page_func( gint current_page , gpointer data )
{
  switch( current_page ){
    case 0:
      return 1;
    case 1:
      if( account_type == _SIP )
	return 2;
      return 4;
    case 2:  
      return 3;
    case 3:
      return 6;
    case 4:
      return 6;
    default:
      return -1;
  }
}

  static GtkWidget*
create_vbox(GtkAssistantPageType type, const gchar *title, const gchar *section)
{
  GtkWidget *vbox;
  GtkWidget *label;
  GdkPixbuf *pixbuf;
  gchar *str;

  vbox = gtk_vbox_new(FALSE, 6);
  gtk_container_set_border_width(GTK_CONTAINER(vbox), 24);

  gtk_assistant_append_page(GTK_ASSISTANT(wiz->assistant), vbox);
  gtk_assistant_set_page_type(GTK_ASSISTANT(wiz->assistant), vbox, type);
  str = g_strdup_printf(" %s", title);
  gtk_assistant_set_page_title(GTK_ASSISTANT(wiz->assistant), vbox, str);

  g_free(str);

  gtk_assistant_set_page_complete(GTK_ASSISTANT(wiz->assistant), vbox, TRUE);

  wiz->logo = gdk_pixbuf_new_from_file(ICONS_DIR "/sflphone.png", NULL);
  gtk_assistant_set_page_header_image(GTK_ASSISTANT(wiz->assistant),vbox, wiz->logo);
  g_object_unref(wiz->logo);

  if (section) {
    label = gtk_label_new(NULL);
    str = g_strdup_printf("<b>%s</b>\n", section);
    gtk_label_set_markup(GTK_LABEL(label), str);
    g_free(str);
    gtk_misc_set_alignment(GTK_MISC(label), 0, 0);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  }
  return vbox;
}

