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
#include <mainwindow.h>
#include <configwindow.h>
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
    "title", "About SFLPhone",
    "version", VERSION,
    "website", "http://www.sflphone.org",
    "copyright", "Copyright © 2004-2007 Savoir-faire Linux Inc.",
    "translator-credits", "Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.net>", 
    "comments", "SFLPhone is a VOIP client compatible with SIP and IAX protocols.",
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
call_quit ( void * foo)
{
  sflphone_quit();
}

void 
call_preferences ( void * foo)
{
  show_config_window();
}

GtkWidget * 
create_call_menu()
{
  GtkWidget * menu;
  GtkWidget * root_menu;
  GtkWidget * menu_items;
  
  menu      = gtk_menu_new ();

  menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_PREFERENCES, get_accel_group());
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
                  G_CALLBACK (call_preferences), 
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
debug_hang_up( void* foo)
{
  call_t * c = (call_t*) call_list_get_by_state (CALL_STATE_CURRENT);
  if(c)
  {
    sflphone_hang_up(c);
  }
}

GtkWidget * 
create_debug_menu()
{
  GtkWidget * menu;
  GtkWidget * root_menu;
  GtkWidget * menu_items;
  
  menu      = gtk_menu_new ();

  menu_items = gtk_menu_item_new_with_label ("Hang up current call");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
                  G_CALLBACK (debug_hang_up), 
                  NULL);
  gtk_widget_show (menu_items);
    
  /*menu_items = gtk_menu_item_new_with_label ("Transfert current call");
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
                  G_CALLBACK (debug_transfert), 
                  NULL);
  gtk_widget_show (menu_items);*/
  
  root_menu = gtk_menu_item_new_with_mnemonic ("_Debug");
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

#ifdef DEBUG  
  root_menu = create_debug_menu();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), root_menu);
#endif  

  root_menu = create_help_menu();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), root_menu);

  
  
  gtk_widget_show (menu_bar);
  
  
  return menu_bar;
}

