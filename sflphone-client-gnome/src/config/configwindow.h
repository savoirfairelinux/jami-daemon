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

#ifndef __CONFIGWINDOW_H__
#define __CONFIGWINDOW_H__

#include <calllist.h>

/**
 * @file configwindow.h
 * @brief The Preferences window.
 */

/**
 * Fill the account list widget with the data the server send
 */
void config_window_fill_account_list();

/**
 * Fill the codec list widget with the data the server send
 */
void config_window_fill_codec_list();

/**
 * Fill the input audio plugin list widget with the data the server send
 * Currently not used
 */
void config_window_fill_input_audio_plugin_list();

/**
 * Fill the output audio plugin list widget with the data the server send
 */
void config_window_fill_output_audio_plugin_list();

/**
 * Fill the output audio device list widget with the data the server send
 */
void config_window_fill_output_audio_device_list();

/**
 * Select an output audio device
 */
void select_active_output_audio_device();

/**
 * Fill the input audio device list widget with the data the server send
 */
void config_window_fill_input_audio_device_list();

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
void update_combo_box( gchar* plugin );

/**
 * Build the widget to display codec list
 * @return GtkWidget* The widget created
 */
GtkWidget * create_codec_table();

/**
 * Create the main account window in a new window
 * @return GtkWidget* The widget created
 */
GtkWidget * create_accounts_tab();

/**
 * Create the audio configuration tab and add it to the main configuration window
 * @return GtkWidget* The widget created
 */
GtkWidget * create_audio_tab();

/**
 * Create the recording configuration tab and add it to the main configuration window
 */
GtkWidget * create_recording_settings();

/**
 * Display the main configuration window
 */
void show_config_window();

/**
 * Display the main account window
 */
void show_accounts_window();

void config_window_set_stun_visible();

void save_configuration_parameters (void);

void history_load_configuration (void);

#endif 
