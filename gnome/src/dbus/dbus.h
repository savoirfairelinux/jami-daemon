/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */

#ifndef __DBUS_H__
#define __DBUS_H__

#include <dbus/dbus-glib.h>

#include <accountlist.h>
#include <calllist.h>
#include <conferencelist.h>
#include <conference_obj.h>
#include <sflnotify.h>

/** @file dbus.h
  * @brief General DBus functions wrappers.
  */

/**
 * Try to connect to DBus services
 * @return TRUE if connection succeeded, FALSE otherwise
 */
gboolean dbus_connect (GError **error);

/**
 * Unreferences the proxies
 */
void dbus_clean ();

/**
 * CallManager - Hold a call
 * @param c The call to hold
 */
void dbus_hold (const callable_obj_t * c);

/**
 * CallManager - Unhold a call
 * @param c The call to unhold
 */
void dbus_unhold (const callable_obj_t * c);

/**
 * CallManager - Hang up a call
 * @param c The call to hang up
 */
void dbus_hang_up (const callable_obj_t * c);

/**
 * CallManager - Transfer a call
 * @param c The call to transfer
 */
void dbus_transfert (const callable_obj_t * c);

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
void dbus_accept (const callable_obj_t * c);

/**
 * CallManager - Refuse a call
 * @param c The call to refuse
 */
void dbus_refuse (const callable_obj_t * c);

/**
 * CallManager - Place a call
 * @param c The call to place
 */
void dbus_place_call (const callable_obj_t * c);


/**
 * ConfigurationManager - Get the list of the setup accounts
 * @return gchar** The list of accounts
 */
gchar ** dbus_account_list();

/**
 * ConfigurationManager - Get the details of a specific account
 * @param accountID The unique of the account
 * @return GHashTable* The details of the account
 */
GHashTable * dbus_get_account_details (gchar * accountID);

/**
 * ConfigurationManager - Set the details of a specific account
 * @param a The account to update
 */
void dbus_set_account_details (account_t *a);

/**
 * ConfigurationManager - Set the additional credential information
 * of a specific account.
 * This function will add the new section on the server side
 * if it cannot be found.
 * @param a The account to update
 */
void dbus_set_credentials (account_t *a);

/**
 * ConfigurationManager - Set the additional credential information
 * of a specific account.
 * This function will add the new section on the server side
 * if it cannot be found.
 * @param a The account to update
 */
void dbus_get_credentials (account_t *a);

/**
 * ConfigurationManager - Get the details for the ip2ip profile
 */
GHashTable * dbus_get_ip2_ip_details (void);

/**
 * ConfigurationManager - Set the details for the ip2ip profile
 */
void dbus_set_ip2ip_details (GHashTable * properties);

/**
 * ConfigurationManager - Send registration request
 * @param accountID The account to register/unregister
 * @param enable The flag for the type of registration
 *		 0 for unregistration request
 *		 1 for registration request
 */
void dbus_send_register (gchar* accountID , const guint enable);

/**
 * ConfigurationManager - Add an account to the list
 * @param a The account to add
 */
void dbus_add_account (account_t *a);

/**
 * ConfigurationManager - Remove an account from the list
 * @param accountID The account to remove
 */
void dbus_remove_account (gchar * accountID);

/**
 * ConfigurationManager - Set volume for speaker/mic
 * @param device The speaker or the mic
 * @param value The new value
 */
void dbus_set_volume (const gchar * device, gdouble value);

/**
 * ConfigurationManager - Get the volume of a device
 * @param device The speaker or the mic
 */
gdouble dbus_get_volume (const gchar * device);

/**
 * ConfigurationManager - Play DTMF
 * @param key The DTMF to send
 */
void dbus_play_dtmf (const gchar * key);

/**
 * ConfigurationManager - Get the audio codecs list
 * @return gchar** The list of audiocodecs
 */
gchar** dbus_audio_codec_list();

/**
 * ConfigurationManager - Get the video codecs list
 * @return gchar** The list of video codecs 
 */
gchar** dbus_video_codec_list();

/**
 * ConfigurationManager - Get the audio codec details
 * @param payload The payload of the audio codec
 * @return gchar** The audio codec details
 */
gchar** dbus_audio_codec_details (int payload);

/**
 * ConfigurationManager - Get the video codec details
 * @param codec The name of the video codec
 * @return gchar** The video codec details
 */
gchar** dbus_video_codec_details (gchar *codec);

/**
 * ConfigurationManager - Get the default audio codec list
 * The default audio codec list are the audio codecs selected by the server if the user hasn't made any changes
 * @return gchar** The default audio codec list
 */
gchar** dbus_default_audio_codec_list();

/**
 * ConfigurationManager - Get the list of the audio codecs used for media negotiation
 * @return gchar** The list of audio codecs
 */
gchar** dbus_get_active_audio_codec_list (gchar *accountID);

/**
 * ConfigurationManager - Set the list of audio codecs used for media negociation
 * @param list The list of audio codecs
 */
void dbus_set_active_audio_codec_list (const gchar** list, const gchar*);

/**
 * ConfigurationManager - Get the list of the audio codecs used for media negotiation
 * @return gchar** The list of audio codecs
 */
gchar** dbus_get_active_video_codec_list (gchar *accountID);

/**
 * ConfigurationManager - Set the list of audio codecs used for media negociation
 * @param list The list of audio codecs
 */
void dbus_set_active_video_codec_list (const gchar** list, const gchar*);

/**
 * CallManager - return the video codec name
 * @param callable_obj_t* current call
 */
gchar* dbus_get_current_video_codec_name (const callable_obj_t * c);

/**
 * CallManager - return the audio codec name
 * @param callable_obj_t* current call
 */
gchar* dbus_get_current_audio_codec_name (const callable_obj_t * c);

/**
 * ConfigurationManager - Get the list of available output audio plugins
 * @return gchar** The list of plugins
 */
gchar** dbus_get_audio_plugin_list();


/**
 * ConfigurationManager - Select an input audio plugin
 * @param audioPlugin The string description of the plugin
 */
void dbus_set_audio_plugin (gchar* audioPlugin);

/**
 * ConfigurationManager - Select an input audio plugin
 * @param audioPlugin The string description of the plugin
 */
void dbus_set_input_audio_plugin (gchar* audioPlugin);

/**
 * ConfigurationManager - Select an output audio plugin
 * @param audioPlugin The string description of the plugin
 */
void dbus_set_output_audio_plugin (gchar* audioPlugin);

/**
 * ConfigurationManager - Get the list of available output audio devices
 * @return gchar** The list of devices
 */
gchar** dbus_get_audio_output_device_list();

/**
 * ConfigurationManager - Select an output audio device
 * @param index The index of the soundcard
 */
void dbus_set_audio_output_device (const int index);

/**
 * ConfigurationManager - Get the list of available input audio devices
 * @return gchar** The list of devices
 */
gchar** dbus_get_audio_input_device_list();

/**
 * ConfigurationManager - Select an input audio device
 * @param index The index of the soundcard
 */
void dbus_set_audio_input_device (const int index);

/**
 * ConfigurationManager - Get the current audio devices
 * @return gchar** The index of the current soundcard
 */
gchar** dbus_get_current_audio_devices_index();

/**
 * ConfigurationManager - Get the index of the specified audio device
 * @param name The string description of the audio device
 * @return int The index of the device
 */
int dbus_get_audio_device_index (const gchar* name);

/**
 * ConfigurationManager - Get the current output audio plugin
 * @return gchar* The current plugin
 *		  default
 *		  plughw
 *		  dmix
 */
gchar* dbus_get_current_audio_output_plugin();

/**
 * ConfigurationManager - Get the current noise suppressor state
 * @return gchar* The state (enabled/disabled)
 */
gchar *dbus_get_noise_suppress_state (void);

/**
 * ConfigurationManager - Set the current noise suppressor state
 * @param gchar* The state (enabled/disabled)
 */
void dbus_set_noise_suppress_state (gchar *state);

/**
 * ConfigurationManager - Get the current echo cancel state
 * @return gchar* The state (enabled/disabled)
 */
gchar *dbus_get_echo_cancel_state(void);

/**
 * ConfigurationManager - Set the current echo cancel state
 * @param gchar* The state (enabled/disabled)
 */
void dbus_set_echo_cancel_state(gchar *state);

int dbus_get_echo_cancel_tail_length(void);

void dbus_set_echo_cancel_tail_length(int length);

int dbus_get_echo_cancel_delay(void);

void dbus_set_echo_cancel_delay(int delay);

/**
 * ConfigurationManager - Query to server to
 * know if MD5 credential hashing is enabled.
 * @return True if enabled, false otherwise
 *
 */
gboolean dbus_is_md5_credential_hashing();

/**
 * ConfigurationManager - Set whether or not
 * the server should store credential as
 * a md5 hash.
 * @param enabled
 */
void dbus_set_md5_credential_hashing (gboolean enabled);

/**
 * ConfigurationManager - Tells the GUI if IAX2 support is enabled
 * @return int 1 if IAX2 is enabled
 *	       0 otherwise
 */
int dbus_is_iax2_enabled (void);

/**
 * ConfigurationManager - Query the server about the ringtone option.
 * If ringtone is enabled, ringtone on incoming call use custom choice. If not, only standart tone.
 * @return int	1 if enabled
 *	        0 otherwise
 */
int dbus_is_ringtone_enabled (const gchar *accountID);

/**
 * ConfigurationManager - Set the ringtone option
 * Inverse current value
 */
void dbus_ringtone_enabled (const gchar *accountID);

/**
 * ConfigurationManager - Get the ringtone
 * @return gchar* The file name selected as a ringtone
 */
gchar* dbus_get_ringtone_choice (const gchar *accountID);

/**
 * ConfigurationManager - Set a ringtone
 * @param tone The file name of the ringtone
 */
void dbus_set_ringtone_choice (const gchar *accountID, const gchar* tone);

/**
 * ConfigurationManager - Gives the maximum number of days the user wants to have in the history
 * @return double The maximum number of days
 */
guint dbus_get_history_limit (void);

/**
 * ConfigurationManager - Gives the maximum number of days the user wants to have in the history
 */
void dbus_set_history_limit (const guint days);

/**
 * ConfigurationManager - Returns the selected audio manager
 * @return int	0	ALSA
 *		1	PULSEAUDIO
 */
int dbus_get_audio_manager (void);

/**
 * ConfigurationManager - Set the audio manager
 * @param api	0	ALSA
 *		1	PULSEAUDIO
 */
void dbus_set_audio_manager (int api);

void dbus_set_video_input_device (const gchar *dev);
void dbus_set_video_input_device_channel (const gchar *channel);
void dbus_set_video_input_size (const char *size);
void dbus_set_video_input_rate (const gchar *rate);
gchar *dbus_get_video_input_device ();
gchar *dbus_get_video_input_device_channel ();
gchar *dbus_get_video_input_device_size ();
gchar *dbus_get_video_input_device_rate ();
gchar **dbus_get_video_input_device_list();
gchar **dbus_get_video_input_device_channel_list(const gchar *dev);
gchar **dbus_get_video_input_device_size_list(const gchar *dev, const gchar *channel);
gchar **dbus_get_video_input_device_rate_list(const gchar *dev, const gchar *channel, const gchar *size);


/**
 * ConfigurationManager - Start a tone when a new call is open and no numbers have been dialed
 * @param start 1 to start
 *		0 to stop
 * @param type  TONE_WITH_MESSAGE
 *		TONE_WITHOUT_MESSAGE
 */
void dbus_start_tone (const int start , const guint type);

/**
 * Instance - Send registration request to dbus service.
 * Manage the instances of clients connected to the server
 * @param pid The pid of the processus client
 * @param name The string description of the client. Here : GTK+ Client
 * @param error return location for a GError or NULL
 */
gboolean dbus_register (int pid, gchar * name, GError **error);

/**
 * Instance - Send unregistration request to dbus services
 * @param pid The pid of the processus
 */
void dbus_unregister (int pid);

void dbus_set_sip_address (const gchar* address);

gint dbus_get_sip_address (void);


/**
 * Add a participant (callID) to this conference (confID)
 */
void dbus_add_participant (const gchar* callID, const gchar* confID);

/**
 * Return a list of participant for this conference (confID)
 */
gchar** dbus_get_participant_list (const gchar *confID);

/**
 * Toggle recording for this instance, may be call or conference
 */
void dbus_set_record (const gchar * id);

/**
 * Set the path where the recorded audio files will be stored
 */
void dbus_set_record_path (const gchar *path);

/**
 * Get the path where the recorded audio files are stored
 */
gchar* dbus_get_record_path (void);

/**
 * Set the always recording functionality, once true all call
 * will be set in recording mode once answered
 */
void dbus_set_is_always_recording(const gboolean);

/**
 * Test if the always recording functionality is activated
 * @return true if call are always recording
 */
gboolean dbus_get_is_always_recording(void);

/**
 * Encapsulate all the address book-related configuration
 * Get the configuration
 */
GHashTable* dbus_get_addressbook_settings (void);

/**
 * Encapsulate all the address book-related configuration
 * Set the configuration
 */
void dbus_set_addressbook_settings (GHashTable *);

gchar** dbus_get_addressbook_list (void);

void dbus_set_addressbook_list (const gchar** list);

/**
 * Resolve the local address given an interface name
 */
gchar * dbus_get_address_from_interface_name (gchar* interface);

/**
 * Query the daemon to return a list of network interface (described as there IP address)
 */
gchar** dbus_get_all_ip_interface (void);

/**
 * Query the daemon to return a list of network interface (described as there name)
 */
gchar** dbus_get_all_ip_interface_by_name (void);

/**
 * Encapsulate all the url hook-related configuration
 * Get the configuration
 */
GHashTable* dbus_get_hook_settings (void);

/**
 * Encapsulate all the url hook-related configuration
 * Set the configuration
 */
void dbus_set_hook_settings (GHashTable *);


gboolean dbus_get_is_recording (const callable_obj_t *);

GHashTable* dbus_get_call_details (const gchar* callID);

gchar** dbus_get_call_list (void);

GHashTable* dbus_get_conference_details (const gchar* confID);

gchar** dbus_get_conference_list (void);

void dbus_set_accounts_order (const gchar* order);

/**
 * Get a list of serialized hisotry entries
 * @return The list of history entries
 */
gchar **dbus_get_history (void);

/**
 * Set the history entries into the daemon. The daemon then write teh content 
 * of this list into the history file
 * @param A list of serialized history entries
 */
void dbus_set_history (gchar **);

void sflphone_display_transfer_status (const gchar* message);

/**
 * CallManager - Confirm Short Authentication String
 * for a given callId
 * @param c The call to confirm SAS
 */
void dbus_confirm_sas (const callable_obj_t * c);

/**
 * CallManager - Reset Short Authentication String
 * for a given callId
 * @param c The call to reset SAS
 */
void dbus_reset_sas (const callable_obj_t * c);

/**
 * CallManager - Request Go Clear in the ZRTP Protocol
 * for a given callId
 * @param c The call that we want to go clear
 */
void dbus_request_go_clear (const callable_obj_t * c);

/**
 * CallManager - Accept Go Clear request from remote
 * for a given callId
 * @param c The call to confirm
 */
void dbus_set_confirm_go_clear (const callable_obj_t * c);

/**
 * CallManager - Get the list of supported TLS methods from
 * the server in textual form.
 * @return an array of string representing supported methods
 */
gchar** dbus_get_supported_tls_method();

GHashTable* dbus_get_shortcuts (void);

void dbus_set_shortcuts (GHashTable * shortcuts);

void dbus_set_audio_ringtone_device (const int index);

void
dbus_hang_up_conference (const conference_obj_t * c);

void
dbus_hold_conference (const conference_obj_t * c);

void
dbus_unhold_conference (const conference_obj_t * c);

void
dbus_detach_participant (const gchar* callID);

void
dbus_join_participant (const gchar* sel_callID, const gchar* drag_callID);

void
dbus_create_conf_from_participant_list(const gchar **list);

void
dbus_join_conference (const gchar* sel_confID, const gchar* drag_confID);

void
dbus_add_main_participant (const gchar* confID);

/* Instant messaging */
void dbus_send_text_message (const gchar* callID, const gchar *message);

/* Video calibration */
void dbus_start_video_preview ();

void dbus_stop_video_preview ();

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


#endif
