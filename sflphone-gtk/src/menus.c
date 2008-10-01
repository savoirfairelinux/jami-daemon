/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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
#include <calltab.h>
#include <assistant.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <string.h> // for strlen

GtkWidget * pickUpMenu;
GtkWidget * hangUpMenu;
GtkWidget * newCallMenu;
GtkWidget * holdMenu;
GtkWidget * copyMenu;
GtkWidget * pasteMenu;

guint holdConnId;     //The hold_menu signal connection ID

GtkWidget * dialpadMenu;
GtkWidget * volumeMenu;
GtkWidget * searchbarMenu;


void update_menus()
{ 
  //Block signals for holdMenu
  gtk_signal_handler_block(GTK_OBJECT(holdMenu), holdConnId);

  gtk_widget_set_sensitive( GTK_WIDGET(pickUpMenu), FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(newCallMenu),FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(holdMenu),   FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(copyMenu),   FALSE);

  call_t * selectedCall = call_get_selected(active_calltree);
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
        gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( holdMenu ), gtk_image_new_from_file( ICONS_DIR "/icon_unhold.svg"));
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
        gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( holdMenu ), gtk_image_new_from_file( ICONS_DIR "/icon_hold.svg"));
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
    "Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>", 
    "Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>"
    "Jean-Philippe Barrette-LaPierre",
    "Laurielle Lea",
    NULL};
  gchar *artists[] = {
    "Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>", 
    "Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>",
    NULL};

  gtk_show_about_dialog( GTK_WINDOW(get_main_window()),
      "artists", artists,
      "authors", authors,
      "comments", _("SFLphone is a VoIP client compatible with SIP and IAX2 protocols."),
      "copyright", "Copyright © 2004-2008 Savoir-faire Linux Inc.",
      "name", PACKAGE,
      "title", _("About SFLphone"),
      "version", VERSION,
      "website", "http://www.sflphone.org",
      NULL);
}


  GtkWidget * 
create_help_menu()
{
  GtkWidget * menu;
  GtkWidget * root_menu;
  GtkWidget * menu_items;

  menu      = gtk_menu_new ();

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
#if GTK_CHECK_VERSION(2,10,0)
  gtk_widget_hide(GTK_WIDGET( get_main_window() ));
  set_minimized( TRUE );
#endif
}

  static void
switch_account(  GtkWidget* item , gpointer data )
{
  account_t* acc = g_object_get_data( G_OBJECT(item) , "account" );
  g_print("%s\n" , acc->accountID);
  account_list_set_current_id( acc->accountID );
}

  static void 
call_hold  (void* foo)
{
  call_t * selectedCall = call_get_selected(current_calls);
  
  if(selectedCall)
  {
    if(selectedCall->state == CALL_STATE_HOLD)
    {
      gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( holdMenu ), gtk_image_new_from_file( ICONS_DIR "/icon_unhold.svg"));
      sflphone_off_hold();
    }
    else
    {
      gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( holdMenu ), gtk_image_new_from_file( ICONS_DIR "/icon_hold.svg"));
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

  static void 
call_wizard ( void * foo)
{
#if GTK_CHECK_VERSION(2,10,0)
  build_wizard();
#endif
}

static void
remove_from_history( void * foo )
{
  call_t* c = call_get_selected( history );
  if(c){
    g_print("Remove the call from the history\n");
    call_list_remove_from_history( c );
  }
}

static void
call_back( void * foo )
{
  call_t* selectedCall = call_get_selected( history );
  call_t* newCall =  g_new0 (call_t, 1);
  if( selectedCall )
  {
    newCall->to = g_strdup(call_get_number(selectedCall));
    newCall->from = g_strconcat("\"\" <", call_get_number(selectedCall), ">",NULL);
    newCall->state = CALL_STATE_DIALING;
    newCall->callID = g_new0(gchar, 30);
    g_sprintf(newCall->callID, "%d", rand()); 
    newCall->_start = 0;
    newCall->_stop = 0;
    call_list_add(current_calls, newCall);
    update_call_tree_add(current_calls, newCall);
    sflphone_place_call(newCall);
    switch_tab();
  } 
}
    
  GtkWidget * 
create_call_menu()
{
  GtkWidget * menu;
  GtkWidget * root_menu;
  GtkWidget * menu_items;
  GtkWidget * image;

  menu      = gtk_menu_new ();

  image = gtk_image_new_from_file( ICONS_DIR "/icon_call.svg");
  newCallMenu = gtk_image_menu_item_new_with_mnemonic(_("_New call"));
  gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( newCallMenu ), image );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), newCallMenu);
  g_signal_connect_swapped (G_OBJECT (newCallMenu), "activate",
      G_CALLBACK (call_new_call), 
      NULL);
  gtk_widget_show (newCallMenu);

  menu_items = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);

  image = gtk_image_new_from_file( ICONS_DIR "/icon_accept.svg");
  pickUpMenu = gtk_image_menu_item_new_with_mnemonic(_("_Pick up"));
  gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( pickUpMenu ), image );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), pickUpMenu);
  gtk_widget_set_sensitive( GTK_WIDGET(pickUpMenu), FALSE);
  g_signal_connect_swapped (G_OBJECT (pickUpMenu), "activate",
      G_CALLBACK (call_pick_up), 
      NULL);
  gtk_widget_show (pickUpMenu);

  image = gtk_image_new_from_file( ICONS_DIR "/icon_hangup.svg");
  hangUpMenu = gtk_image_menu_item_new_with_mnemonic(_("_Hang up"));
  gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( hangUpMenu ), image );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), hangUpMenu);
  gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), FALSE);
  g_signal_connect_swapped (G_OBJECT (hangUpMenu), "activate",
      G_CALLBACK (call_hang_up), 
      NULL);
  gtk_widget_show (hangUpMenu);

  image = gtk_image_new_from_file( ICONS_DIR "/icon_hold.svg");
  holdMenu = gtk_image_menu_item_new_with_mnemonic (_("On _Hold"));
  gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( holdMenu ), image );
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

#if GTK_CHECK_VERSION(2,10,0)
  menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Account Assistant"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  g_signal_connect_swapped( G_OBJECT( menu_items ) , "activate" , G_CALLBACK( call_wizard  ) , NULL );
  gtk_widget_show (menu_items);
  // Separator
  menu_items = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
#endif

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
  call_t * selectedCall = call_get_selected(current_calls);
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
  call_t * selectedCall = call_get_selected(current_calls);
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
	  update_call_tree(current_calls, selectedCall);
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

	  update_call_tree(current_calls, selectedCall);
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
	    update_call_tree(current_calls, selectedCall);

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
    update_call_tree(current_calls,selectedCall);
  }

}

  static void
clear_history( void* foo )
{
  gchar *markup;
  GtkWidget *dialog;
  int response;

  if( call_list_get_size( history ) == 0 ){
    markup = g_markup_printf_escaped(_("History empty"));
    dialog = gtk_message_dialog_new_with_markup ( GTK_WINDOW(get_main_window()),
							    GTK_DIALOG_DESTROY_WITH_PARENT,
							    GTK_MESSAGE_INFO,
							    GTK_BUTTONS_CLOSE,
							    markup);
    response = gtk_dialog_run (GTK_DIALOG(dialog));
    gtk_widget_destroy (GTK_WIDGET(dialog));
  }
  else{  
      call_list_clean_history();
  }
}

  GtkWidget * 
create_edit_menu()
{
  GtkWidget * menu;
  GtkWidget * image;
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

  menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Clear history"));
  image = gtk_image_new_from_stock( GTK_STOCK_CLEAR , GTK_ICON_SIZE_MENU );
  gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( menu_items ), image );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
      G_CALLBACK (clear_history), 
      NULL);
  gtk_widget_show (menu_items);  

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
view_dialpad  (GtkImageMenuItem *imagemenuitem,
    void* foo)
{
  gboolean state;
  main_window_dialpad( &state );
  if( state )
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( dialpadMenu ),
				  gtk_image_new_from_file( ICONS_DIR "/icon_dialpad_off.svg"));
  else	
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( dialpadMenu ), 
				  gtk_image_new_from_file( ICONS_DIR "/icon_dialpad.svg"));
  dbus_set_dialpad( state );
  

}

  static void 
view_volume_controls  (GtkImageMenuItem *imagemenuitem,
    void* foo)
{
  gboolean state;
  main_window_volume_controls( &state );
  if( state )
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( volumeMenu ),
				  gtk_image_new_from_file( ICONS_DIR "/icon_volume_off.svg"));
  else	
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( volumeMenu ), 
				  gtk_image_new_from_file( ICONS_DIR "/icon_volume.svg"));
  dbus_set_volume_controls( state );
}

  static void 
view_searchbar  (GtkImageMenuItem *imagemenuitem,
    void* foo)
{
  gboolean state;
  main_window_searchbar( &state );
  dbus_set_searchbar( state );
}

  GtkWidget * 
create_view_menu()
{
  GtkWidget * menu;
  GtkWidget * root_menu;
  GtkWidget * image;

  menu      = gtk_menu_new ();

  if( SHOW_DIALPAD )
    image = gtk_image_new_from_file( ICONS_DIR "/icon_dialpad_off.svg");
  else
    image = gtk_image_new_from_file( ICONS_DIR "/icon_dialpad.svg");
  dialpadMenu = gtk_image_menu_item_new_with_mnemonic (_("_Dialpad"));
  gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( dialpadMenu ), image );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), dialpadMenu);
  g_signal_connect(G_OBJECT ( dialpadMenu ), "activate",
      G_CALLBACK (view_dialpad), 
      NULL);
  gtk_widget_show (dialpadMenu);

  if( SHOW_VOLUME )
    image = gtk_image_new_from_file( ICONS_DIR "/icon_volume.svg");
  else
    image = gtk_image_new_from_file( ICONS_DIR "/icon_volume.svg");
  volumeMenu = gtk_image_menu_item_new_with_mnemonic (_("_Volume controls"));
  gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( volumeMenu ), image );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), volumeMenu);
  g_signal_connect(G_OBJECT (volumeMenu), "activate",
      G_CALLBACK (view_volume_controls), 
      NULL);
  gtk_widget_show (volumeMenu);

  image = gtk_image_new_from_stock( GTK_STOCK_FIND , GTK_ICON_SIZE_MENU );
  searchbarMenu = gtk_image_menu_item_new_with_mnemonic (_("_Search history"));
  gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( searchbarMenu ), image );
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), searchbarMenu);
  g_signal_connect(G_OBJECT (searchbarMenu), "activate",
      G_CALLBACK (view_searchbar), 
      NULL);
  gtk_widget_show (searchbarMenu);

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

  call_t * selectedCall = call_get_selected(current_calls);
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
  GtkWidget *image;
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
    image = gtk_image_new_from_file( ICONS_DIR "/icon_accept.svg");
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_items), image);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    g_signal_connect (G_OBJECT (menu_items), "activate",
	G_CALLBACK (call_pick_up), 
	NULL);
    gtk_widget_show (menu_items);
  }

  if(hangup)
  {
    menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Hang up"));
    image = gtk_image_new_from_file( ICONS_DIR "/icon_hangup.svg");
    gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_items), image);
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
      // Display only the registered accounts
      if( g_strcasecmp( account_state_name(acc -> state) , account_state_name(ACCOUNT_STATE_REGISTERED) ) == 0 ){
	alias = g_strconcat( g_hash_table_lookup(acc->properties , ACCOUNT_ALIAS) , " - ",g_hash_table_lookup(acc->properties , ACCOUNT_TYPE), NULL);
	menu_items = gtk_check_menu_item_new_with_mnemonic(alias);
	gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
	g_object_set_data( G_OBJECT( menu_items ) , "account" , acc );
	g_free( alias );
	if( account_list_get_current() != NULL ){
	  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items),
	      (g_strcasecmp( acc->accountID , account_list_get_current()->accountID) == 0)? TRUE : FALSE);
	}
	g_signal_connect (G_OBJECT (menu_items), "activate",
	    G_CALLBACK (switch_account), 
	    NULL);
	gtk_widget_show (menu_items);
      } // fi
    }
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


void
show_popup_menu_history(GtkWidget *my_widget, GdkEventButton *event)
{

  gboolean pickup = FALSE;
  gboolean remove = FALSE;

  call_t * selectedCall = call_get_selected( history );
  if (selectedCall)
  {
    remove = TRUE;
    pickup = TRUE;
  } 

  GtkWidget *menu;
  GtkWidget *image;
  int button, event_time;
  GtkWidget * menu_items;

  menu = gtk_menu_new ();
  //g_signal_connect (menu, "deactivate", 
  //       G_CALLBACK (gtk_widget_destroy), NULL);

  if(pickup)
  {

    menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Call back"));
    image = gtk_image_new_from_file( ICONS_DIR "/icon_accept.svg");
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( menu_items ), image );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    g_signal_connect (G_OBJECT (menu_items), "activate",G_CALLBACK (call_back), NULL);
    gtk_widget_show (menu_items);
  }

  menu_items = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  gtk_widget_show (menu_items);

  if(remove)
  {
    menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_DELETE, get_accel_group());
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    g_signal_connect (G_OBJECT (menu_items), "activate", G_CALLBACK (remove_from_history),  NULL);
    gtk_widget_show (menu_items);
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
