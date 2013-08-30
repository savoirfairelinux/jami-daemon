/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#ifndef __STATUSICON_H__
#define __STATUSICON_H__

#include <gtk/gtk.h>
#include "sflphone_client.h"

/**
 * @file statusicon.h
 * @brief The status icon in the system tray.
 */

/**
 * Popup the main window. Used on incoming calls
 */
void popup_main_window(SFLPhoneClient *client);

/**
 * Create the system tray icon
 */
void show_status_icon(SFLPhoneClient *client);


/**
 * Hide the system tray icon
 */
void hide_status_icon ();

/**
 * Set the menu active
 */
void status_icon_unminimize();

/**
 * Show hangup icon
 */
void show_status_hangup_icon(SFLPhoneClient *client);


/**
 * Show hangup icon
 */
void hide_status_hangup_icon();

/**
 * Toggle window minimization
 * @param state  TRUE to minimize the main window
 *               FALSE otherwise
 */
void set_minimized(gboolean state);

/**
 * Make the system tray icon blink on incoming call
 * @return active TRUE to make it blink
 *		  FALSE to make it stop
 */
void status_tray_icon_blink (gboolean active);

/**
 * Accessor
 * @return GtkStatusIcon* The status icon
 */
GtkStatusIcon* get_status_icon (void);

/**
 * Attach a tooltip to the status icon
 */
void statusicon_set_tooltip (void);

void status_tray_icon_online (gboolean online);

#endif
