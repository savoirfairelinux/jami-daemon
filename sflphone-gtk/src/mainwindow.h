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
 
#ifndef __MAINWINDOW_H__
#define __MAINWINDOW_H__

#include <calllist.h>

GtkAccelGroup * get_accel_group();
GtkWidget * get_main_window();

void create_main_window ( );

void main_window_ask_quit() ;
/**
  * Shows the dialpad on the mainwindow 
  * @param show TRUE if you want to show the dialpad, FALSE to hide it
  */
void main_window_dialpad(gboolean show);

void main_window_error_message(gchar * markup);

void main_window_warning_message(gchar * markup);

void main_window_warning_message(gchar * markup);
#endif 
