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


#include <gtk/gtk.h>
#include <actions.h>
#include <mainwindow.h>
#include <statusicon.h>

GtkStatusIcon* status;
gboolean minimized = FALSE;
void 
status_quit ( void * foo)
{
  sflphone_quit();
}

void 
activate (GtkStatusIcon *status_icon, void * foo)
{
  if(minimized)
  {
    gtk_widget_show(GTK_WIDGET(get_main_window()));
  }   
  else
  {
    gtk_widget_hide(GTK_WIDGET(get_main_window()));
  }
  minimized = !minimized;
}

void menu (GtkStatusIcon *status_icon,
            guint button,
            guint activate_time,
            GtkWidget * menu) 
{
  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL, button, activate_time);
}

GtkWidget * 
create_menu()
{
  GtkWidget * menu;
  GtkWidget * menu_items;
  
  menu      = gtk_menu_new ();
  
  menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_QUIT, get_accel_group());
  g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
                  G_CALLBACK (status_quit), 
                  NULL);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
  gtk_widget_show (menu_items);
  
  return menu;
}

void
show_status_icon()
{
  status = gtk_status_icon_new_from_file(ICON_DIR "/sflphone.png");
  g_signal_connect (G_OBJECT (status), "activate",
			  G_CALLBACK (activate),
			  NULL);
  g_signal_connect (G_OBJECT (status), "popup-menu",
			  G_CALLBACK (menu),
			  create_menu());			  
}

