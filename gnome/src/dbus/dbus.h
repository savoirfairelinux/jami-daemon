/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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

#ifndef DBUS_H_
#define DBUS_H_

#include <dbus/dbus-glib.h>

#include "accountlist.h"
#include "calllist.h"
#include "conferencelist.h"
#include "conference_obj.h"
#include "sflnotify.h"
#include "history_loader.h"

/** @file dbus.h
  * @brief General DBus functions wrappers.
  */

/**
 * Try to connect to DBus services
 * @return TRUE if connection succeeded, FALSE otherwise
 */
gboolean dbus_connect(GError **error, SFLPhoneClient *client);

/**
 * Unreferences the proxies
 */
void dbus_clean();

/**
 * CallManager - Hold a call
 * @param c The call to hold
 */
void dbus_hold(const callable_obj_t *c);

/**
 * CallManager - Unhold a call
 * @param c The call to unhold
 */
void dbus_unhold(const callable_obj_t *c);

/**
 * CallManager - Hang up a call
 * @param c The call to hang up
 */
void dbus_hang_up(const callable_obj_t *c);

/**
 * CallManager - Transfer a call
 * @param c The call to transfer
 */
void dbus_transfer(const callable_obj_t *c);

/**
 * CallManager - Perform an attended transfer on two calls
 * @param The call to be transfered
 * @param The target
 */
void dbus_attended_transfer(const callable_obj_t *, const callable_obj_t *);

/**
 * CallManager - Accept a call
 * @param c The call to accept
 */
void dbus_accept(const callable_obj_t *c);

/**
 * CallManager - Refuse a call
 * @param c The call to refuse
 */
void dbus_refuse(const callable_obj_t *c);

/**
 * CallManager - Place a call
 * @param c The call to place
 */
void dbus_place_call(const callable_obj_t *c);


/**
 * ConfigurationManager - Get the list of the setup accounts
 * @return gchar** The list of accounts
 */
gchar **dbus_account_list();

/**
 * configurationmanager - get a template for a new account
 * @return ghashtable* the details of the account
 */
GHashTable *dbus_get_account_template();

/**
 * configurationmanager - get the details of a specific account
 * @param accountid the unique of the account
 * @return ghashtable* the details of the account
 */
GHashTable *dbus_get_account_details(const gchar *accountID);

/**
 * ConfigurationManager - Set the details of a specific account
 * @param a The account to update
 */
void dbus_set_account_details(const account_t *a);

/**
 * ConfigurationManager - Set the additional credential information
 * of a specific account.
 * This function will add the new section on the server side
 * if it cannot be found.
 * @param a The account to update
 */
void dbus_set_credentials(account_t *a);

/**
 * ConfigurationManager - Set the additional credential information
 * of a specific account.
 * This function will add the new section on the server side
 * if it cannot be found.
 * @param a The account to update
 */
void dbus_get_credentials(account_t *a);

/**
 * ConfigurationManager - Get the details for the ip2ip profile
 */
GHashTable *dbus_get_ip2ip_details(void);

/**
 * ConfigurationManager - Send registration request
 * @param accountID The account to register/unregister
 * @param enable The flag for the type of registration
 *		 0 for unregistration request
 *		 1 for registration request
 */
void dbus_send_register(const gchar *accountID, gboolean enable);

/**
 * ConfigurationManager - Add an account to the list
 * @param a The account to add
 */
void dbus_add_account(account_t *a);

/**
 * ConfigurationManager - Remove an account from the list
 * @param accountID The account to remove
 */
void dbus_remove_account(const gchar *accountID);

/**
 * ConfigurationManager - Set volume for speaker/mic
 * @param device The speaker or the mic
 * @param value The new value
 */
void dbus_set_volume(const gchar *device, gdouble value);

/**
 * ConfigurationManager - Get the volume of a device
 * @param device The speaker or the mic
 */
gdouble dbus_get_volume(const gchar *device);

void dbus_mute_capture(gboolean mute);
void dbus_mute_playback(gboolean mute);
gboolean dbus_is_capture_muted();
gboolean dbus_is_playback_muted();

void dbus_mute_dtmf(gboolean mute);
gboolean dbus_is_dtmf_muted();

/**
 * ConfigurationManager - Play DTMF
 * @param key The DTMF to send
 */
void dbus_play_dtmf(const gchar *key);

/**
 * ConfigurationManager - Get the audio codecs list
 * @return gchar** The list of audiocodecs
 */
GArray *dbus_audio_codec_list();

/**
 * ConfigurationManager - Get the audio codec details
 * @param payload The payload of the audio codec
 * @return gchar** The audio codec details
 */
gchar **dbus_audio_codec_details(int payload);

/**
 * ConfigurationManager - Get the video codec details
 * @param codec The name of the video codec
 * @return gchar** The video codec details
 */
GHashTable *dbus_video_codec_details(const gchar *codec);

/**
 * ConfigurationManager - Get the default audio codec list
 * The default audio codec list are the audio codecs selected by the server if the user hasn't made any changes
 * @return gchar** The default audio codec list
 */
gchar **dbus_default_audio_codec_list();

/**
 * ConfigurationManager - Get the list of the audio codecs used for media negotiation
 * @return The list of audio codecs
 */
GArray *dbus_get_active_audio_codec_list(const gchar *accountID);

/**
 * ConfigurationManager - Set the list of audio codecs used for media negociation
 * @param list The list of audio codecs
 */
void dbus_set_active_audio_codec_list(const gchar **list, const gchar *);

/**
 * ConfigurationManager - Get the list of the audio codecs used for media negotiation
 * @return gchar** The list of audio codecs
 */
GPtrArray *
dbus_get_video_codecs(const gchar *accountID);

/**
 * ConfigurationManager - Set the list of audio codecs used for media negociation
 * @param id The accountID
 * @param list The list of codecs
 */
void
dbus_set_video_codecs(const gchar *id, const GPtrArray *list);

/**
 * ConfigurationManager - Switch the video input
 * @param resource A media resource locator (MRL) to switch to
 */
gboolean
dbus_switch_video_input(const gchar *resource);

/**
 * ConfigurationManager - Get the list of available output audio plugins
 * @return gchar** The list of plugins
 */
gchar **dbus_get_audio_plugin_list();


/**
 * ConfigurationManager - Select an input audio plugin
 * @param audioPlugin The string description of the plugin
 */
void dbus_set_audio_plugin(const gchar *audioPlugin);

/**
 * ConfigurationManager - Get the list of available output audio devices
 * @return gchar** The list of devices
 */
gchar **dbus_get_audio_output_device_list();

/**
 * ConfigurationManager - Select an output audio device
 * @param index The index of the soundcard
 */
void dbus_set_audio_output_device(int index);

/**
 * ConfigurationManager - Get the list of available input audio devices
 * @return gchar** The list of devices
 */
gchar **dbus_get_audio_input_device_list();

/**
 * ConfigurationManager - Select an input audio device
 * @param index The index of the soundcard
 */
void dbus_set_audio_input_device(int index);

/**
 * ConfigurationManager - Get the current audio devices
 * @return gchar** The index of the current soundcard
 */
gchar **dbus_get_current_audio_devices_index();

/**
 * ConfigurationManager - Get the index of the specified output audio device
 * @param name The string description of the audio device
 * @return int The index of the device
 */
int dbus_get_audio_output_device_index(const gchar *name);

/**
 * ConfigurationManager - Get the index of the specified input audio device
 * @param name The string description of the audio device
 * @return int The index of the device
 */
int dbus_get_audio_input_device_index(const gchar *name);

/**
 * ConfigurationManager - Get the current output audio plugin
 * @return gchar* The current plugin
 *		  default
 *		  plughw
 *		  dmix
 */
gchar *dbus_get_current_audio_output_plugin();

/**
 * ConfigurationManager - Get the current noise suppressor state
 * @return gboolean The state (enabled/disabled)
 */
gboolean dbus_get_noise_suppress_state(void);

/**
 * ConfigurationManager - Set the current noise suppressor state
 * @param gboolean The state (enabled/disabled)
 */
void dbus_set_noise_suppress_state(gboolean state);

/**
 * ConfigurationManager - Get the current AGC state
 * @return gboolean The state (enabled/disabled)
 */
gboolean dbus_get_agc_state(void);

/**
 * ConfigurationManager - Set the current noise suppressor state
 * @param gboolean The state (enabled/disabled)
 */
void dbus_set_agc_state(gboolean state);

/**
 * ConfigurationManager - Tells the GUI if IAX2 support is enabled
 * @return int 1 if IAX2 is enabled
 *	       0 otherwise
 */
int dbus_is_iax2_enabled(void);

/**
 * ConfigurationManager - Gives the maximum number of days the user wants to have in the history
 * @return double The maximum number of days
 */
guint dbus_get_history_limit(void);

/**
 * ConfigurationManager - Gives the maximum number of days the user wants to have in the history
 */
void dbus_set_history_limit(guint days);

/**
 * ConfigurationManager - Returns the selected audio manager
 * @return "alsa"
 *		or "pulseaudio"
 */
gchar *dbus_get_audio_manager(void);

gchar **dbus_get_supported_audio_managers(void);

/**
 * ConfigurationManager - Set the audio manager
 * @param api	"alsa"
 *		"pulseaudio"
 */
gboolean dbus_set_audio_manager(const gchar *api);

gchar *dbus_video_get_default_device();
void dbus_video_set_default_device(const gchar *name);
GHashTable *dbus_video_get_settings(const gchar *name);
void dbus_video_apply_settings(const gchar *name, GHashTable *settings);
gchar **dbus_video_get_device_list();
GHashTable *dbus_video_get_capabilities(const gchar *name);

/**
 * ConfigurationManager - Start a tone when a new call is open and no numbers have been dialed
 * @param start 1 to start
 *		0 to stop
 * @param type  TONE_WITH_MESSAGE
 *		TONE_WITHOUT_MESSAGE
 */
void dbus_start_tone(int start, guint type);

/**
 * Instance - Send registration request to dbus service.
 * Manage the instances of clients connected to the server
 * @param pid The pid of the processus client
 * @param name The string description of the client. Here : GTK+ Client
 * @param error return location for a GError or NULL
 */
gboolean dbus_register(int pid, const gchar *name, GError **error);

/**
 * Instance - Send unregistration request to dbus services
 * @param pid The pid of the processus
 */
void dbus_unregister(int pid);

void dbus_set_sip_address(const gchar *address);

gint dbus_get_sip_address(void);


/**
 * Add a participant (callID) to this conference (confID)
 */
void dbus_add_participant(const gchar *callID, const gchar *confID);

/**
 * Return a list of display names for this conference (confID)
 */
gchar **
dbus_get_display_names(const gchar *confID);

/**
 * Return a list of participant for this conference (confID)
 */
gchar **dbus_get_participant_list(const gchar *confID);

/**
 * If this call is part of a conference, return the conference id,
 * otherwise return an empty string.
 * Result must be freed by caller.
 */
gchar *dbus_get_conference_id(const gchar *callID);

/**
 * Toggle recording for this instance, may be call or conference
 */
gboolean dbus_toggle_recording(const gchar *id);

/**
 * Set the path where the recorded audio files will be stored
 */
void dbus_set_record_path(const gchar *path);

/**
 * Get the path where the recorded audio files are stored
 */
gchar *dbus_get_record_path(void);

/**
 *
 */
void dbus_set_record_playback_seek(gdouble value);

/**
 * Set the always recording functionality, once true all call
 * will be set in recording mode once answered
 */
void dbus_set_is_always_recording(gboolean);

/**
 * Test if the always recording functionality is activated
 * @return true if call are always recording
 */
gboolean dbus_get_is_always_recording(void);

/**
 * Resolve the local address given an interface name
 */
gchar * dbus_get_address_from_interface_name(const gchar *interface);

/**
 * Query the daemon to return a list of network interface (described as there IP address)
 */
gchar **dbus_get_all_ip_interface(void);

/**
 * Query the daemon to return a list of network interface (described as there name)
 */
gchar **dbus_get_all_ip_interface_by_name(void);

/**
 * Encapsulate all the url hook-related configuration
 * Get the configuration
 */
GHashTable* dbus_get_hook_settings(void);

/**
 * Encapsulate all the url hook-related configuration
 * Set the configuration
 */
void dbus_set_hook_settings(GHashTable *);


gboolean dbus_get_is_recording(const callable_obj_t *);

GHashTable *dbus_get_call_details(const gchar *callID);

/* Returns a newly allocated list callIDs.
 * Caller is responsible for freeing it with g_strfreev */
gchar **dbus_get_call_list(void);

GHashTable* dbus_get_conference_details(const gchar *confID);

/* Returns a newly allocated list conferenceIDs.
 * Caller is responsible for freeing it with g_strfreev */
gchar **dbus_get_conference_list(void);

void dbus_set_accounts_order(const gchar *order);

/**
 * Get a the history
 * @return The PtrArray of history entries
 */
void dbus_get_history(IdleData *id);

void dbus_clear_history(void);

void sflphone_display_transfer_status(const gchar *message);

/**
 * CallManager - Confirm Short Authentication String
 * for a given callId
 * @param c The call to confirm SAS
 */
void dbus_confirm_sas(const callable_obj_t *c);

/**
 * CallManager - Reset Short Authentication String
 * for a given callId
 * @param c The call to reset SAS
 */
void dbus_reset_sas(const callable_obj_t *c);

/**
 * CallManager - Request Go Clear in the ZRTP Protocol
 * for a given callId
 * @param c The call that we want to go clear
 */
void dbus_request_go_clear(const callable_obj_t *c);

/**
 * CallManager - Accept Go Clear request from remote
 * for a given callId
 * @param c The call to confirm
 */
void dbus_set_confirm_go_clear(const callable_obj_t *c);

/**
 * CallManager - Get the list of supported TLS methods from
 * the server in textual form.
 * @return an array of string representing supported methods
 */
gchar **dbus_get_supported_tls_method();

gboolean dbus_certificate_contains_private_key(const gchar *filepath);
gboolean dbus_check_certificate(const gchar *capath,
                                const gchar *certpath);

GHashTable* dbus_get_shortcuts(void);

void dbus_set_shortcuts(GHashTable *shortcuts);

void dbus_set_audio_ringtone_device(int index);

void
dbus_hang_up_conference(const conference_obj_t *c);

void
dbus_hold_conference(const conference_obj_t *c);

void
dbus_unhold_conference(const conference_obj_t *c);

void
dbus_detach_participant(const gchar *callID);

void
dbus_join_participant(const gchar *sel_callID, const gchar *drag_callID);

void
dbus_join_conference(const gchar *sel_confID, const gchar *drag_confID);

void
dbus_add_main_participant(const gchar *confID);

/* Instant messaging */
void dbus_send_text_message(const gchar *callID, const gchar *message);

/* Video calibration */
void dbus_start_video_renderer();

void dbus_stop_video_renderer();

/**
 * Start playback of a recorded
 * @param The recorded file to start playback with
 */
gboolean dbus_start_recorded_file_playback(const gchar *);

/**
 * Stop playback of a recorded filie
 * @param The recorded file to pause
 */
void dbus_stop_recorded_file_playback(const gchar *);

void dbus_start_video_camera();
void dbus_stop_video_camera();
gboolean dbus_has_video_camera_started();

/**
 * Prevent Gnome Session Manager from entering in screen-saver mode
 */
void dbus_screensaver_inhibit(void);

/**
 * Allow Gnome Session Manager to enter in screen-saver mode
 */
void dbus_screensaver_uninhibit(void);


/**
 * Presence methods
 */
void dbus_presence_publish(const gchar *accountID, gboolean status);
void dbus_presence_subscribe(const gchar *accountID, const gchar *uri, gboolean flag);
#endif
