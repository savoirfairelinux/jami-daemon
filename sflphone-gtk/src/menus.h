/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
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
 
#ifndef __MENUS_H__
#define __MENUS_H__

#include <gtk/gtk.h>
/** @file menus.h
  * @brief The menus of the main window.
  */

/**
 * Build the menus bar
 * @return GtkWidget* The menu bar
 */
GtkWidget * create_menus();

/**
 * Update the menu state
 */
void update_menus();

/**
 * Create a menu on right-click
 * @param my_widget The widget you click on
 * @param event The mouse event
 */
void show_popup_menu (GtkWidget *my_widget, GdkEventButton *event);

/**
 * Create a menu on right-click for the history
 * @param my_widget The widget you click on
 * @param event The mouse event
 */
void show_popup_menu_history (GtkWidget *my_widget, GdkEventButton *event);
#endif 
