/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#ifndef __CONFIGWINDOW_H__
#define __CONFIGWINDOW_H__

#include <calllist.h>

/**
 * @file preferencesdialog.h
 * @brief The Preferences window.
 */

/**
 * Update the combo box state.
 * If the default plugin has been selected, the audio devices have to been unsensitive
 * because the default plugin always use default audio device
 * @param plugin The description of the selected plugin
 */
void update_combo_box (gchar* plugin);

/**
 * Build the widget to display codec list
 * @return GtkWidget* The widget created
 */
GtkWidget * create_codec_table();

/**
 * Create the main account window in a new window
 * @return GtkWidget* The widget created
 */
GtkWidget * create_accounts_tab (GtkDialog * dialog);

/**
 * Create the recording configuration tab and add it to the main configuration window
 */
GtkWidget * create_recording_settings();

/**
 * Display the main configuration window
 */
guint show_preferences_dialog();

void preferences_dialog_set_stun_visible();

void save_configuration_parameters (void);

void history_load_configuration (void);

void instant_messaging_load_configuration (void);

GtkTreeModel* createModel();

#endif
