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
 
#include <menus.h>
#include <config.h>
#include <calllist.h>
#include <actions.h>
#include <mainwindow.h>
#include <configwindow.h>

GtkWidget * pickUpMenu;
GtkWidget * hangUpMenu;
GtkWidget * newCallMenu;
GtkWidget * holdMenu;
guint holdConnId;     //The hold_menu signal connection ID

void update_menus()
{ 
  //Block signals for holdMenu
  gtk_signal_handler_block(GTK_OBJECT(holdMenu), holdConnId);
  
  gtk_widget_set_sensitive( GTK_WIDGET(pickUpMenu), FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(newCallMenu),FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(holdMenu),   FALSE);
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(holdMenu), FALSE);
	
	call_t * selectedCall = call_get_selected();
	if (selectedCall)
	{
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
  	    g_warning("Should not happen!");
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
void 
help_about ( void * foo)
{
  gchar *authors[] = {
    "Yan Morin <yan.morin@savoirfairelinux.com>", 
    "Jérôme Oufella <jerome.oufella@savoirfairelinux.com>",
    "Julien Plissonneau Duquene <julien.plissonneau.duquene@savoirfairelinux.com>",
    "Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>",
    "Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.net>", 
    "Imran Akbar", 
    "Jean-Philippe Barrette-LaPierre",
    "Laurielle Lea",
    "Mikael Magnusson",
    "Sherry Yang", 
    NULL};
  gchar *artists[] = {
    "Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.net>", 
    NULL};
  
  gtk_show_about_dialog( GTK_WINDOW(get_main_window()),
    "name", PACKAGE,
    "title", "About SFLphone",
    "version", VERSION,
    "website", "http://www.sflphone.org",
    "copyright", "Copyright © 2004-2007 Savoir-faire Linux Inc.",
    "translator-credits", "Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.net>", 
    "comments", "SFLphone is a VoIP client compatible with SIP and IAX protocols.",
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
    
  root_menu = gtk_menu_item_new_with_mnemonic ("_Help");
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (root_menu), menu);

  return root_menu;
}
/* ----------------------------------------------------------------- */
void 
call_new_call ( void * foo)
{
  sflphone_new_call();
}

void 
call_quit ( void * foo)
{
  sflphone_quit();
}

void 
call_hold  (void* foo)
{
  if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(holdMenu)))
  {
    sflphone_on_hold();
  }
  else
  {
    sflphone_off_hold();
  } 
}

void 
call_pick_up ( void * foo)
{
  sflphone_pick_up();
}

void 
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
 
  newCallMenu = gtk_image_menu_item_new_with_mnemonic("_New call...");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), newCallMenu);
  g_signal_connect_swapped (G_OBJECT (newCallMenu), "activate",
                  G_CALLBACK (call_new_call), 
                  NULL);
  gtk_widget_show (newCallMenu);
  
  menu_items = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  
  pickUpMenu = gtk_image_menu_item_new_with_mnemonic("_Pick up");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), pickUpMenu);
  gtk_widget_set_sensitive( GTK_WIDGET(pickUpMenu), FALSE);
  g_signal_connect_swapped (G_OBJECT (pickUpMenu), "activate",
                  G_CALLBACK (call_pick_up), 
                  NULL);
  gtk_widget_show (pickUpMenu);
  
  hangUpMenu = gtk_image_menu_item_new_with_mnemonic("_Hang up");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), hangUpMenu);
  gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), FALSE);
  g_signal_connect_swapped (G_OBJECT (hangUpMenu), "activate",
                  G_CALLBACK (call_hang_up), 
                  NULL);
  gtk_widget_show (hangUpMenu);
  
  holdMenu = gtk_check_menu_item_new_with_mnemonic ("On _Hold");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), holdMenu);
  gtk_widget_set_sensitive( GTK_WIDGET(holdMenu),   FALSE);
  //Here we connect only to activate
  //The toggled state is managed from update_menus()
  holdConnId = g_signal_connect(G_OBJECT (holdMenu), "activate",
                  G_CALLBACK (call_hold), 
                  NULL);
  gtk_widget_show (menu_items);
  
  menu_items = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  
  menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_QUIT, get_accel_group());
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
                  G_CALLBACK (call_quit), 
                  NULL);
  gtk_widget_show (menu_items);
    
  
  root_menu = gtk_menu_item_new_with_mnemonic ("_Call");
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (root_menu), menu);

  return root_menu;
}
/* ----------------------------------------------------------------- */

void 
edit_preferences ( void * foo)
{
  show_config_window();
}

GtkWidget * 
create_edit_menu()
{
  GtkWidget * menu;
  GtkWidget * root_menu;
  GtkWidget * menu_items;
  
  menu      = gtk_menu_new ();

  menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_COPY, get_accel_group());
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  gtk_widget_set_sensitive( GTK_WIDGET(menu_items),   FALSE);
  gtk_widget_show (menu_items);
  
  menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_PASTE, get_accel_group());
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  gtk_widget_set_sensitive( GTK_WIDGET(menu_items),   FALSE);
  gtk_widget_show (menu_items);
  
  menu_items = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  
  menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_PREFERENCES, get_accel_group());
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
                  G_CALLBACK (edit_preferences), 
                  NULL);
  gtk_widget_show (menu_items);  
    
  
  root_menu = gtk_menu_item_new_with_mnemonic ("_Edit");
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (root_menu), menu);

  return root_menu;
}
/* ----------------------------------------------------------------- */
void 
view_dial_pad  (GtkCheckMenuItem *checkmenuitem,
                void* foo)
{
  main_window_dialpad(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(checkmenuitem)));
}

GtkWidget * 
create_view_menu()
{
  GtkWidget * menu;
  GtkWidget * root_menu;
  GtkWidget * menu_items;
  
  menu      = gtk_menu_new ();

  menu_items = gtk_check_menu_item_new_with_mnemonic ("_Dialpad");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  g_signal_connect(G_OBJECT (menu_items), "toggled",
                  G_CALLBACK (view_dial_pad), 
                  NULL);
  gtk_widget_show (menu_items);
  
  menu_items = gtk_check_menu_item_new_with_mnemonic ("_Volume controls");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  gtk_widget_set_sensitive( GTK_WIDGET(menu_items),   FALSE);
  g_signal_connect(G_OBJECT (menu_items), "toggled",
                  G_CALLBACK (view_dial_pad), 
                  NULL);
  gtk_widget_show (menu_items);
  
  menu_items = gtk_check_menu_item_new_with_mnemonic ("_Toolbar");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  gtk_widget_set_sensitive( GTK_WIDGET(menu_items),   FALSE);
  g_signal_connect(G_OBJECT (menu_items), "toggled",
                  G_CALLBACK (view_dial_pad), 
                  NULL);
  gtk_widget_show (menu_items);
  
  root_menu = gtk_menu_item_new_with_mnemonic ("_View");
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

