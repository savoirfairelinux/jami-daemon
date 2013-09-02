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

#ifndef __CONFIGWINDOW_H__
#define __CONFIGWINDOW_H__

#include "calllist.h"
#include "sflphone_client.h"

/**
 * @file preferencesdialog.h
 * @brief The Preferences window.
 */

/**
 * Fill the input audio plugin list widget with the data the server send
 * Currently not used
 */
void preferences_dialog_fill_input_audio_plugin_list();

/**
 * Fill the output audio plugin list widget with the data the server send
 */
void preferences_dialog_fill_output_audio_plugin_list();

/**
 * Fill the output audio device list widget with the data the server send
 */
void preferences_dialog_fill_output_audio_device_list();

/**
 * Select an output audio device
 */
void select_active_output_audio_device();

/**
 * Fill the input audio device list widget with the data the server send
 */
void preferences_dialog_fill_input_audio_device_list();

/**
 * Select an input audio device
 */
void select_active_input_audio_device();

/**
 * Select an output audio plugin
 */
void select_active_output_audio_plugin();

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
guint show_preferences_dialog(SFLPhoneClient *client);

void preferences_dialog_set_stun_visible();

void save_configuration_parameters(SFLPhoneClient *client);

#endif
