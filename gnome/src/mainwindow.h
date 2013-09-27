/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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

#include "calllist.h"
#include "calltree.h"
#include "uimanager.h"

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
GtkWidget *waitingLayer;

/**
 * Build the main window
 */
void
create_main_window(SFLPhoneClient *client);

/**
 * Display a dialog window
 * Ask the user if he wants to hangup current calls before quiting
 * @return gboolean TRUE if the user wants to hang up
 *		    FALSE otherwise
 */
gboolean main_window_ask_quit(SFLPhoneClient *client);

/**
  * Shows/Hides the dialpad on the mainwindow
  */
void main_window_dialpad (gboolean state, SFLPhoneClient *client);

/**
  * Shows/Hides the dialpad on the mainwindow
  */
void main_window_volume_controls (gboolean state);

/**
 * Push a message on the statusbar stack
 * @param left_hand_message The message to display on the left side
 * @param right_hand_message The message to display on the right side
 * @param id  The identifier of the message
 */
void statusbar_push_message (const gchar * const left_hand_message, const gchar * const right_hand_message, guint id);

void statusbar_enable_presence();

/**
 * Pop a message from the statusbar stack
 * @param id  The identifier of the message
 */
void statusbar_pop_message (guint id);

/**
 * Update selected call's clock in statusbar
 * @param id  The identifier of the message
 */
void statusbar_update_clock (const gchar * time);

void main_window_zrtp_not_supported(callable_obj_t * c, SFLPhoneClient *client);

void main_window_zrtp_negotiation_failed(const gchar* callID, const gchar* reason, const gchar* severity, SFLPhoneClient *client);

void main_window_confirm_go_clear(callable_obj_t * c, SFLPhoneClient *client);

void focus_on_searchbar_out();
void focus_on_searchbar_in();

/**
 * Given the current position and the full length of the file, update the playback position
 * if the size is 0 or if the current value is larger than the size, the cursor position
 * is moved at the end of the scale.
 */
gboolean main_window_update_playback_scale(const gchar *file_path, guint current, guint size);

void main_window_set_playback_scale_sensitive();

void main_window_set_playback_scale_unsensitive();

/**
 * Show the playback scale, (should occur if the user selected an history entry witch has been recorded)
 */
void main_window_show_playback_scale();

/**
 * Hide the playback scale
 */
void main_window_hide_playback_scale();

/**
 * Pause the key grabber while an other widget is focussed
 */
void main_window_pause_keygrabber(gboolean value);

void main_window_reset_playback_scale();

void main_window_bring_to_front(SFLPhoneClient *client, guint32 timestamp);

void main_window_update_seekslider(const gchar *recordfile);

#endif
