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

#include <gtk/gtk.h>
#include <actions.h>
#include <mainwindow.h>
#include <accountlist.h>
#include <statusicon.h>

GtkStatusIcon* status;
GtkWidget * show_menu_item;
gboolean minimized = FALSE;

void 
status_quit ( void * foo)
{
  sflphone_quit();
}

void 
status_icon_unminimize()
{
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(show_menu_item), TRUE);
}

void 
show_hide (GtkWidget *menu, void * foo)
{
  if(gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(show_menu_item)))
  {
    gtk_widget_show(GTK_WIDGET(get_main_window()));
  }   
  else
  {
    gtk_widget_hide(GTK_WIDGET(get_main_window()));
  }
}


void 
status_click (GtkStatusIcon *status_icon, void * foo)
{
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(show_menu_item), 
    !gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(show_menu_item)));
}

void menu (GtkStatusIcon *status_icon,
            guint button,
            guint activate_time,
            GtkWidget * menu) 
{
  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, gtk_status_icon_position_menu, 
    status_icon, button, activate_time);
}

GtkWidget * 
create_menu()
{
  GtkWidget * menu;
  GtkWidget * menu_items;
  
  menu      = gtk_menu_new ();
  
  show_menu_item = gtk_check_menu_item_new_with_mnemonic (_("_Show main window"));
  gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(show_menu_item), TRUE);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), show_menu_item);
  g_signal_connect(G_OBJECT (show_menu_item), "toggled",
                  G_CALLBACK (show_hide), 
                  NULL);
                  
  menu_items = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  
  menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_QUIT, get_accel_group());
  g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
                  G_CALLBACK (status_quit), 
                  NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  
  gtk_widget_show_all (menu);
  
  return menu;
}

void
show_status_icon()
{
  status = gtk_status_icon_new_from_file(ICON_DIR "/sflphone.png");
  g_signal_connect (G_OBJECT (status), "activate",
			  G_CALLBACK (status_click),
			  NULL);
  g_signal_connect (G_OBJECT (status), "popup-menu",
			  G_CALLBACK (menu),
			  create_menu());			  

  // Add a tooltip to the system tray icon
  gchar* tip = malloc(500);
  sprintf( tip , _("SFLphone - %i accounts registered") , account_list_get_size());
  gtk_status_icon_set_tooltip( status , tip );
  g_free(tip);
}

void
status_tray_icon_blink(  )
{
  gtk_status_icon_set_blinking( status , !gtk_status_icon_get_blinking( status ) );
}
