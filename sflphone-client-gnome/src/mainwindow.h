/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef __MAINWINDOW_H__
#define __MAINWINDOW_H__

#include <calllist.h>
#include <calltree.h>
#include <uimanager.h>

#define MAIN_WINDOW_WIDTH 280
#define MAIN_WINDOW_HEIGHT 320

/** @file mainwindow.h
  * @brief The main window of the client.
  */
GtkAccelGroup * get_accel_group();

/**
 * Display the main window
 * @return GtkWidget* The main window
 */
GtkWidget * get_main_window();

GtkWidget *waitingLayer;

/**
 * Build the main window
 */
void create_main_window ( );

/**
 * Display a dialog window
 * Ask the user if he wants to hangup current calls before quiting
 * @return gboolean TRUE if the user wants to hang up
 *		    FALSE otherwise
 */
gboolean main_window_ask_quit() ;

/**
  * Shows/Hides the dialpad on the mainwindow
  */
void main_window_dialpad( gboolean state );

/**
  * Shows/Hides the dialpad on the mainwindow
  */
void main_window_volume_controls( gboolean state );

/**
 * Display an error message
 * @param markup  The error message
 */
void main_window_error_message(gchar * markup);

/**
 * Display a warning message
 * @param markup  The warning message
 */
void main_window_warning_message(gchar * markup);

/**
 * Display an info message
 * @param markup  The info message
 */
void main_window_info_message(gchar * markup);

/**
 * Push a message on the statusbar stack
 * @param message The message to display
 * @param id  The identifier of the message
 */
void statusbar_push_message( const gchar* message , guint id );

/**
 * Pop a message from the statusbar stack
 * @param id  The identifier of the message
 */
void statusbar_pop_message( guint id );

//static gboolean
//on_key_released (GtkWidget *widget, GdkEventKey *event,
//                 gpointer user_data);
// void set_focus_on_mainwindow();

gboolean focus_is_on_calltree;

gboolean focus_is_on_searchbar;

void main_window_zrtp_not_supported(callable_obj_t * c);

void main_window_zrtp_negotiation_failed(const gchar* callID, const gchar* reason, const gchar* severity);

void main_window_confirm_go_clear(callable_obj_t * c);

#endif
