/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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
 
#ifndef __STATUSICON_H__
#define __STATUSICON_H__


#include <gtk/gtk.h>
#include <sflphone_const.h>
/** 
 * @file statusicon.h
 * @brief The status icon in the system tray.
 */

/**
 * Create the status icon 
 */
void show_status_icon();

/**
 * Set the menu active 
 */  
void status_icon_unminimize();

/**
 * Tells if the main window if minimized or not
 * @return gboolean TRUE if the main window is minimized
 *		    FALSE otherwise
 */
gboolean main_widget_minimized();

/**
 * Change the menu status
 * @param state	TRUE if the  main window is minimized
 *               FALSE otherwise
 */
void set_minimized( gboolean state );

/**
 * Make the system tray icon blink on incoming call
 * @return active TRUE to make it blink
 *		  FALSE to make it stop
 */
void status_tray_icon_blink( gboolean active );

/**
 * Accessor
 * @return GtkStatusIcon* The status icon
 */
GtkStatusIcon* get_status_icon( void );

#endif
