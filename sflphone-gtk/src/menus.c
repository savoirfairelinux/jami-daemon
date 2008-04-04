/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc@squidy.info>
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

#include <menus.h>
#include <actions.h>
#include <calllist.h>
#include <calltree.h>
#include <config.h>
#include <configwindow.h>
#include <dbus.h>
#include <mainwindow.h>
#include <screen.h>
#include <gtk/gtk.h>

#include <string.h> // for strlen

GtkWidget * pickUpMenu;
GtkWidget * hangUpMenu;
GtkWidget * newCallMenu;
GtkWidget * holdMenu;
GtkWidget * copyMenu;
GtkWidget * pasteMenu;
guint holdConnId;     //The hold_menu signal connection ID

void update_menus()
{ 
  //Block signals for holdMenu
  gtk_signal_handler_block(GTK_OBJECT(holdMenu), holdConnId);

  gtk_widget_set_sensitive( GTK_WIDGET(pickUpMenu), FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(newCallMenu),FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(holdMenu),   FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(copyMenu),   FALSE);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(holdMenu), FALSE);

  call_t * selectedCall = call_get_selected();
  if (selectedCall)
  {
    gtk_widget_set_sensitive( GTK_WIDGET(copyMenu),   TRUE);
    switch(selectedCall->state) 
    {
      case CALL_STATE_INCOMING:
	gtk_widget_set_sensitive( GTK_WIDGET(pickUpMenu), TRUE);
	gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), TRUE);
	break;
      case CALL_STATE_HOLD:
	gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), TRUE);
	gtk_widget_set_sensitive( GTK_WIDGET(holdMenu),   TRUE);
	gtk_widget_set_sensitive( GTK_WIDGET(newCallMenu),TRUE);
	gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(holdMenu), TRUE);
	break;
      case CALL_STATE_RINGING:
	gtk_widget_set_sensitive( GTK_WIDGET(pickUpMenu), TRUE);
	gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), TRUE);
	break;
      case CALL_STATE_DIALING:
	gtk_widget_set_sensitive( GTK_WIDGET(pickUpMenu), TRUE);
	gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), TRUE);
	gtk_widget_set_sensitive( GTK_WIDGET(newCallMenu),TRUE);
	break;
      case CALL_STATE_CURRENT:
	gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), TRUE);
	gtk_widget_set_sensitive( GTK_WIDGET(holdMenu),   TRUE);
	gtk_widget_set_sensitive( GTK_WIDGET(newCallMenu),TRUE);
	break;
      case CALL_STATE_BUSY:
      case CALL_STATE_FAILURE:
	gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), TRUE);
	break; 
      default:
	g_warning("Should not happen in update_menus()!");
	break;
    }
  } 
  else
  {
    gtk_widget_set_sensitive( GTK_WIDGET(newCallMenu), TRUE);
  }
  gtk_signal_handler_unblock(holdMenu, holdConnId);

}
/* ----------------------------------------------------------------- */
  static void 
help_about ( void * foo)
{
  gchar *authors[] = {
    "Yan Morin <yan.morin@savoirfairelinux.com>", 
    "Jérôme Oufella <jerome.oufella@savoirfairelinux.com>",
    "Julien Plissonneau Duquene <julien.plissonneau.duquene@savoirfairelinux.com>",
    "Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>",
    "Pierre-Luc Beaudoin <pierre-luc@squidy.info>", 
    "Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>"
      "Jean-Philippe Barrette-LaPierre",
    "Laurielle Lea",
    NULL};
  gchar *artists[] = {
    "Pierre-Luc Beaudoin <pierre-luc@squidy.info>", 
    NULL};

  gtk_show_about_dialog( GTK_WINDOW(get_main_window()),
      "name", PACKAGE,
      "title", _("About SFLphone"),
      "version", VERSION,
      "website", "http://www.sflphone.org",
      "copyright", "Copyright © 2004-2007 Savoir-faire Linux Inc.",
      "translator-credits", "", 
      "comments", _("SFLphone is a VoIP client compatible with SIP and IAX2 protocols."),
      "artists", artists,
      "authors", authors,
      NULL);
}


  GtkWidget * 
create_help_menu()
{
  GtkWidget * menu;
  GtkWidget * root_menu;
  GtkWidget * menu_items;

  menu      = gtk_menu_new ();

  /*menu_items = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    */
  menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_ABOUT, get_accel_group());
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
      G_CALLBACK (help_about), 
      NULL);
  gtk_widget_show (menu_items);

  root_menu = gtk_menu_item_new_with_mnemonic (_("_Help"));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (root_menu), menu);

  return root_menu;
}
/* ----------------------------------------------------------------- */
  static void 
call_new_call ( void * foo)
{
  sflphone_new_call();
}

  static void 
call_quit ( void * foo)
{
  sflphone_quit();
}

  static void 
call_minimize ( void * foo)
{
  gtk_widget_hide(GTK_WIDGET( get_main_window() ));
  set_minimized( TRUE );
}

  static void
switch_account(  GtkWidget* item , gpointer data )
{
  account_t* acc = g_object_get_data( G_OBJECT(item) , "account" );
  g_print("%s\n" , acc->accountID);
  account_list_set_current( acc->accountID );
}

  static void 
call_hold  (void* foo)
{
  call_t * selectedCall = call_get_selected();

  if(selectedCall)
  {
    if(selectedCall->state == CALL_STATE_HOLD)
    {
      sflphone_off_hold();
    }
    else
    {
      sflphone_on_hold();
    } 
  } 
}

  static void 
call_pick_up ( void * foo)
{
  sflphone_pick_up();
}

  static void 
call_hang_up ( void * foo)
{
  sflphone_hang_up();
}

  GtkWidget * 
create_call_menu()
{
  GtkWidget * menu;
  GtkWidget * root_menu;
  GtkWidget * menu_items;

  menu      = gtk_menu_new ();

  newCallMenu = gtk_image_menu_item_new_with_mnemonic(_("_New call"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), newCallMenu);
  g_signal_connect_swapped (G_OBJECT (newCallMenu), "activate",
      G_CALLBACK (call_new_call), 
      NULL);
  gtk_widget_show (newCallMenu);

  menu_items = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);

  pickUpMenu = gtk_image_menu_item_new_with_mnemonic(_("_Pick up"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), pickUpMenu);
  gtk_widget_set_sensitive( GTK_WIDGET(pickUpMenu), FALSE);
  g_signal_connect_swapped (G_OBJECT (pickUpMenu), "activate",
      G_CALLBACK (call_pick_up), 
      NULL);
  gtk_widget_show (pickUpMenu);

  hangUpMenu = gtk_image_menu_item_new_with_mnemonic(_("_Hang up"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), hangUpMenu);
  gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), FALSE);
  g_signal_connect_swapped (G_OBJECT (hangUpMenu), "activate",
      G_CALLBACK (call_hang_up), 
      NULL);
  gtk_widget_show (hangUpMenu);

  holdMenu = gtk_check_menu_item_new_with_mnemonic (_("On _Hold"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), holdMenu);
  gtk_widget_set_sensitive( GTK_WIDGET(holdMenu),   FALSE);
  //Here we connect only to activate
  //The toggled state is managed from update_menus()
  holdConnId = g_signal_connect(G_OBJECT (holdMenu), "activate",
      G_CALLBACK (call_hold), 
      NULL);
  gtk_widget_show (menu_items);

  // Separator
  menu_items = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);

  // Close menu to minimize the main window to the system tray
  menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_CLOSE, get_accel_group());
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
      G_CALLBACK (call_minimize), 
      NULL);
  gtk_widget_show (menu_items);

  // Separator
  menu_items = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);

  // Quit Menu - quit SFLphone
  menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_QUIT, get_accel_group());
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
      G_CALLBACK (call_quit), 
      NULL);
  gtk_widget_show (menu_items);


  root_menu = gtk_menu_item_new_with_mnemonic (_("_Call"));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (root_menu), menu);

  return root_menu;
}
/* ----------------------------------------------------------------- */

  static void 
edit_preferences ( void * foo)
{
  show_config_window();
}

  static void 
edit_accounts ( void * foo)
{
  show_accounts_window();
}

// The menu Edit/Copy should copy the current selected call's number
  static void 
edit_copy ( void * foo)
{
  GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  call_t * selectedCall = call_get_selected();
  gchar * no = NULL;

  if(selectedCall)
  {
    switch(selectedCall->state)
    {
      case CALL_STATE_TRANSFERT:  
      case CALL_STATE_DIALING:
      case CALL_STATE_RINGING:
	no = selectedCall->to;
	break;
      case CALL_STATE_CURRENT:
      case CALL_STATE_HOLD:
      case CALL_STATE_BUSY:
      case CALL_STATE_FAILURE:
      case CALL_STATE_INCOMING:
      default:
	no = call_get_number(selectedCall);
	break;
    }

    gtk_clipboard_set_text (clip, no, strlen(no) );
  }

}

// The menu Edit/Paste should paste the clipboard into the current selected call
  static void 
edit_paste ( void * foo)
{
  GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
  call_t * selectedCall = call_get_selected();
  gchar * no = gtk_clipboard_wait_for_text (clip);

  if(no && selectedCall)
  {
    switch(selectedCall->state)
    {
      case CALL_STATE_TRANSFERT:  
      case CALL_STATE_DIALING:
	// Add the text to the number
	{ 
	  gchar * before = selectedCall->to;
	  selectedCall->to = g_strconcat(selectedCall->to, no, NULL);
	  g_free(before);
	  g_print("TO: %s\n", selectedCall->to);

	  if(selectedCall->state == CALL_STATE_DIALING)
	  {
	    g_free(selectedCall->from);
	    selectedCall->from = g_strconcat("\"\" <", selectedCall->to, ">", NULL);
	  }
	  screen_set_call(selectedCall);
	  update_call_tree(selectedCall);
	}
	break;
      case CALL_STATE_RINGING:  
      case CALL_STATE_INCOMING:
      case CALL_STATE_BUSY:
      case CALL_STATE_FAILURE:
      case CALL_STATE_HOLD:
	{ // Create a new call to hold the new text
	  selectedCall = sflphone_new_call();

	  gchar * before = selectedCall->to;
	  selectedCall->to = g_strconcat(selectedCall->to, no, NULL);
	  g_free(before);
	  g_print("TO: %s\n", selectedCall->to);

	  g_free(selectedCall->from);
	  selectedCall->from = g_strconcat("\"\" <", selectedCall->to, ">", NULL);

	  screen_set_call(selectedCall);
	  update_call_tree(selectedCall);
	}
	break;
      case CALL_STATE_CURRENT:
      default:
	{
	  int i;
	  for(i = 0; i < strlen(no); i++)
	  {
	    gchar * oneNo = g_strndup(&no[i], 1);
	    g_print("<%s>\n", oneNo);
	    dbus_play_dtmf(oneNo);

	    gchar * temp = g_strconcat(call_get_number(selectedCall), oneNo, NULL);
	    gchar * before = selectedCall->from;
	    selectedCall->from = g_strconcat("\"",call_get_name(selectedCall) ,"\" <", temp, ">", NULL);
	    g_free(before);
	    g_free(temp);
	    screen_set_call(selectedCall);
	    update_call_tree(selectedCall);

	  }
	}
	break;
    }

  }
  else // There is no current call, create one
  {
    selectedCall = sflphone_new_call();

    gchar * before = selectedCall->to;
    selectedCall->to = g_strconcat(selectedCall->to, no, NULL);
    g_free(before);
    g_print("TO: %s\n", selectedCall->to);

    g_free(selectedCall->from);
    selectedCall->from = g_strconcat("\"\" <", selectedCall->to, ">", NULL);

    screen_set_call(selectedCall);
    update_call_tree(selectedCall);
  }

}

  GtkWidget * 
create_edit_menu()
{
  GtkWidget * menu;
  GtkWidget * root_menu;
  GtkWidget * menu_items;

  menu      = gtk_menu_new ();

  copyMenu = gtk_image_menu_item_new_from_stock( GTK_STOCK_COPY, get_accel_group());
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), copyMenu);
  g_signal_connect_swapped (G_OBJECT (copyMenu), "activate",
      G_CALLBACK (edit_copy), 
      NULL);
  gtk_widget_show (copyMenu);

  pasteMenu = gtk_image_menu_item_new_from_stock( GTK_STOCK_PASTE, get_accel_group());
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), pasteMenu);
  g_signal_connect_swapped (G_OBJECT (pasteMenu), "activate",
      G_CALLBACK (edit_paste), 
      NULL);
  gtk_widget_show (pasteMenu);

  menu_items = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);

  menu_items = gtk_menu_item_new_with_mnemonic( _("_Accounts") );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
      G_CALLBACK (edit_accounts), 
      NULL);
  gtk_widget_show (menu_items);  

  menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_PREFERENCES, get_accel_group());
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
      G_CALLBACK (edit_preferences), 
      NULL);
  gtk_widget_show (menu_items);  


  root_menu = gtk_menu_item_new_with_mnemonic (_("_Edit"));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (root_menu), menu);

  return root_menu;
}
/* ----------------------------------------------------------------- */
  static void 
view_dialpad  (GtkCheckMenuItem *checkmenuitem,
    void* foo)
{
  main_window_dialpad(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(checkmenuitem)));
  dbus_set_dialpad(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(checkmenuitem)));
}

  GtkWidget * 
create_view_menu()
{
  GtkWidget * menu;
  GtkWidget * root_menu;
  GtkWidget * menu_items;
  GtkWidget * submenu;
  GtkWidget * submenu_items;

  menu      = gtk_menu_new ();

  menu_items = gtk_check_menu_item_new_with_mnemonic (_("_Dialpad"));
  gtk_check_menu_item_set_active( GTK_CHECK_MENU_ITEM ( menu_items ), dbus_get_dialpad());
  main_window_dialpad(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(menu_items)));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  g_signal_connect(G_OBJECT (menu_items), "toggled",
      G_CALLBACK (view_dialpad), 
      NULL);
  gtk_widget_show (menu_items);

  menu_items = gtk_check_menu_item_new_with_mnemonic (_("_Volume controls"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  gtk_widget_set_sensitive( GTK_WIDGET(menu_items),   FALSE);
  gtk_widget_show (menu_items);

  menu_items = gtk_menu_item_new_with_mnemonic(_("_Toolbar"));
  gtk_widget_set_sensitive( GTK_WIDGET(menu_items) , FALSE);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);

  root_menu = gtk_menu_item_new_with_mnemonic (_("_View"));
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (root_menu), menu);

  return root_menu;
}
/* ----------------------------------------------------------------- */
  GtkWidget * 
create_menus ( )
{

  GtkWidget * menu_bar;
  GtkWidget * root_menu;

  menu_bar  = gtk_menu_bar_new ();

  root_menu = create_call_menu();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), root_menu);

  root_menu = create_edit_menu();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), root_menu);

  root_menu = create_view_menu();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), root_menu);

  root_menu = create_help_menu();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), root_menu);

  gtk_widget_show (menu_bar);


  return menu_bar;
}

/* ----------------------------------------------------------------- */

  void
show_popup_menu (GtkWidget *my_widget, GdkEventButton *event)
{
  // TODO update the selection to make sure the call under the mouse is the call selected

  gboolean pickup = FALSE, hangup = FALSE, hold = FALSE, copy = FALSE;
  gboolean accounts = FALSE;

  call_t * selectedCall = call_get_selected();
  if (selectedCall)
  {
    copy = TRUE;
    switch(selectedCall->state) 
    {
      case CALL_STATE_INCOMING:
	pickup = TRUE;
	hangup = TRUE;
	break;
      case CALL_STATE_HOLD:
	hangup = TRUE;
	hold   = TRUE;
	break;
      case CALL_STATE_RINGING:
	hangup = TRUE;
	break;
      case CALL_STATE_DIALING:
	pickup = TRUE;
	hangup = TRUE;
	accounts = TRUE;
	break;
      case CALL_STATE_CURRENT:
	hangup = TRUE;
	hold   = TRUE;
	break;
      case CALL_STATE_BUSY:
      case CALL_STATE_FAILURE:
	hangup = TRUE;
	break; 
      default:
	g_warning("Should not happen in show_popup_menu!");
	break;
    }
  } 

  GtkWidget *menu;
  //GtkWidget *image;
  int button, event_time;
  GtkWidget * menu_items;

  menu = gtk_menu_new ();
  //g_signal_connect (menu, "deactivate", 
  //       G_CALLBACK (gtk_widget_destroy), NULL);

  if(copy)
  {
    menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_COPY, get_accel_group());
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    g_signal_connect (G_OBJECT (menu_items), "activate",
	G_CALLBACK (edit_copy), 
	NULL);
    gtk_widget_show (menu_items);
  }

  menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_PASTE, get_accel_group());
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  g_signal_connect (G_OBJECT (menu_items), "activate",
      G_CALLBACK (edit_paste), 
      NULL);
  gtk_widget_show (menu_items);

  if(pickup || hangup || hold)
  {
    menu_items = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    gtk_widget_show (menu_items);
  }

  if(pickup)
  {

    menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Pick up"));
    //image = gtk_image_new_from_file( ICONS_DIR "/accept.svg");
    //gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_items), image);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    g_signal_connect (G_OBJECT (menu_items), "activate",
	G_CALLBACK (call_pick_up), 
	NULL);
    gtk_widget_show (menu_items);
  }

  if(hangup)
  {
    menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Hang up"));
    //image = gtk_image_new_from_file( ICONS_DIR "/hang_up.svg");
    //gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_items), image);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    g_signal_connect (G_OBJECT (menu_items), "activate",
	G_CALLBACK (call_hang_up), 
	NULL);
    gtk_widget_show (menu_items);
  }

  if(hold)
  {
    menu_items = gtk_check_menu_item_new_with_mnemonic (_("On _Hold"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items), 
	(selectedCall->state == CALL_STATE_HOLD ? TRUE : FALSE));
    g_signal_connect(G_OBJECT (menu_items), "activate",
	G_CALLBACK (call_hold), 
	NULL);
    gtk_widget_show (menu_items);
  }  

  if(accounts)
  {
    menu_items = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    gtk_widget_show (menu_items);

    int i;
    account_t* acc;
    gchar* alias;
    for( i = 0 ; i < account_list_get_size() ; i++ ){
      acc = account_list_get_nth(i);
      if( g_strcasecmp( account_state_name(acc -> state) , account_state_name(ACCOUNT_STATE_REGISTERED) ) == 0 ){
	alias = g_strconcat( g_hash_table_lookup(acc->properties , ACCOUNT_ALIAS) , " - ",g_hash_table_lookup(acc->properties , ACCOUNT_TYPE), NULL);
	menu_items = gtk_check_menu_item_new_with_mnemonic(alias);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
	g_object_set_data( G_OBJECT( menu_items ) , "account" , acc );
	if( account_list_get_current() == NULL ){
	  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items),
	      (g_strcasecmp( acc->accountID , account_list_get_default()) == 0)? TRUE : FALSE);
	}
	else{
	  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items),
	      (g_strcasecmp( acc->accountID , account_list_get_current()) == 0)? TRUE : FALSE);
	}
	g_signal_connect (G_OBJECT (menu_items), "activate",
	    G_CALLBACK (switch_account), 
	    NULL);
	gtk_widget_show (menu_items);
      }
    }
    g_free( alias );
  }

  if (event)
  {
    button = event->button;
    event_time = event->time;
  }
  else
  {
    button = 0;
    event_time = gtk_get_current_event_time ();
  }

  gtk_menu_attach_to_widget (GTK_MENU (menu), my_widget, NULL);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, 
      button, event_time);
}

