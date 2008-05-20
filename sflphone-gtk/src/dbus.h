/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
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
 */


#ifndef __DBUS_H__
#define __DBUS_H__

#include <accountlist.h>
#include <calllist.h>
#include <sflnotify.h>

/** @file dbus.h
  * @brief General DBus functions wrappers.
  */

/** 
 * Try to connect to DBus services
 * @return TRUE if connection succeeded, FALSE otherwise 
 */
gboolean dbus_connect ();

/**
 * Unreferences the proxies
 */
void dbus_clean ();

/**
 * CallManager - Hold a call
 * @param c The call to hold
 */
void dbus_hold (const call_t * c );

/**
 * CallManager - Unhold a call
 * @param c The call to unhold
 */
void dbus_unhold (const call_t * c );

/**
 * CallManager - Hang up a call
 * @param c The call to hang up
 */
void dbus_hang_up (const call_t * c );

/**
 * CallManager - Transfer a call
 * @param c The call to transfer
 */
void dbus_transfert (const call_t * c);

/**
 * CallManager - Accept a call
 * @param c The call to accept
 */
void dbus_accept (const call_t * c);

/**
 * CallManager - Refuse a call
 * @param c The call to refuse
 */
void dbus_refuse (const call_t * c);

/**
 * CallManager - Place a call
 * @param c The call to place
 */
void dbus_place_call (const call_t * c);


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
GHashTable * dbus_account_details(gchar * accountID);

/**
 * ConfigurationManager - Set the details of a specific account
 * @param a The account to update 
 */
void dbus_set_account_details(account_t *a);

/**
 * ConfigurationManager - Send registration request
 * @param accountID The account to register/unregister 
 * @param expire The flag for the type of registration
 *		 0 for unregistration request
 *		 1 for registration request
 */
void dbus_send_register( gchar* accountID , const guint expire );

/**
 * ConfigurationManager - Add an account to the list
 * @param a The account to add  
 */
void dbus_add_account(account_t *a);

/**
 * ConfigurationManager - Remove an account from the list
 * @param accountID The account to remove  
 */
void dbus_remove_account(gchar * accountID);

/**
 * ConfigurationManager - Set volume for speaker/mic 
 * @param device The speaker or the mic
 * @param value The new value
 */
void dbus_set_volume(const gchar * device, gdouble value);

/**
 * ConfigurationManager - Get the volume of a device
 * @param device The speaker or the mic
 */
gdouble dbus_get_volume(const gchar * device);

/**
 * ConfigurationManager - Play DTMF
 * @param key The DTMF to send 
 */
void dbus_play_dtmf(const gchar * key);

/**
 * ConfigurationManager - Get the codecs list 
 * @return gchar** The list of codecs 
 */
gchar** dbus_codec_list();

/**
 * ConfigurationManager - Get the codec details 
 * @param payload The payload of the codec
 * @return gchar** The codec details
 */
gchar** dbus_codec_details(int payload);

/**
 * ConfigurationManager - Get the default codec list
 * The default codec list are the codecs selected by the server if the user hasn't made any changes 
 * @return gchar** The default codec list 
 */
gchar** dbus_default_codec_list();

/**
 * ConfigurationManager - Get the list of the codecs used for media negociation
 * @return gchar** The list of codecs
 */
gchar** dbus_get_active_codec_list( void );

/**
 * ConfigurationManager - Set the list of codecs used for media negociation
 * @param list The list of codecs
 */
void dbus_set_active_codec_list( const gchar** list );

/**
 * ConfigurationManager - Get the list of available input audio plugins
 * @return gchar** The list of plugins
 */
gchar** dbus_get_input_audio_plugin_list();

/**
 * ConfigurationManager - Get the list of available output audio plugins
 * @return gchar** The list of plugins
 */
gchar** dbus_get_output_audio_plugin_list();

/**
 * ConfigurationManager - Select an input audio plugin
 * @param audioPlugin The string description of the plugin
 */
void dbus_set_input_audio_plugin(gchar* audioPlugin);

/**
 * ConfigurationManager - Select an output audio plugin
 * @param audioPlugin The string description of the plugin
 */
void dbus_set_output_audio_plugin(gchar* audioPlugin);

/**
 * ConfigurationManager - Get the list of available output audio devices
 * @return gchar** The list of devices
 */
gchar** dbus_get_audio_output_device_list();

/**
 * ConfigurationManager - Select an output audio device
 * @param index The index of the soundcard
 */
void dbus_set_audio_output_device(const int index);

/**
 * ConfigurationManager - Get the list of available input audio devices
 * @return gchar** The list of devices
 */
gchar** dbus_get_audio_input_device_list();

/**
 * ConfigurationManager - Select an input audio device
 * @param index The index of the soundcard
 */
void dbus_set_audio_input_device(const int index);

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
int dbus_get_audio_device_index(const gchar* name);

/**
 * ConfigurationManager - Get the current output audio plugin
 * @return gchar* The current plugin 
 *		  default
 *		  plughw
 *		  dmix
 */
gchar* dbus_get_current_audio_output_plugin();

/**
 * ConfigurationManager - Tells the GUI if IAX2 support is enabled
 * @return int 1 if IAX2 is enabled
 *	       0 otherwise
 */
int dbus_is_iax2_enabled( void );

/**
 * ConfigurationManager - Query the server about the ringtone option.
 * If ringtone is enabled, ringtone on incoming call use custom choice. If not, only standart tone. 
 * @return int	1 if enabled
 *	        0 otherwise
 */
int dbus_is_ringtone_enabled( void );

/**
 * ConfigurationManager - Set the ringtone option 
 * Inverse current value
 */
void dbus_ringtone_enabled( void );

/**
 * ConfigurationManager - Get the ringtone 
 * @return gchar* The file name selected as a ringtone
 */
gchar* dbus_get_ringtone_choice( void );

/**
 * ConfigurationManager - Set a ringtone
 * @param tone The file name of the ringtone
 */
void dbus_set_ringtone_choice( const gchar* tone );

/**
 * ConfigurationManager - Set the dialpad visible or not
 */
void dbus_set_dialpad(  );

/**
 * ConfigurationManager - Tells if the user wants to display the dialpad or not 
 * @return int 1 if dialpad has to be displayed
 *	       0 otherwise
 */
int dbus_get_dialpad( void );

/**
 * ConfigurationManager - Set the searchbar visible or not
 */
void dbus_set_searchbar(  );

/**
 * ConfigurationManager - Tells if the user wants to display the search bar or not 
 * @return int 1 if the search bar has to be displayed
 *	       0 otherwise
 */
int dbus_get_searchbar( void );

/**
 * ConfigurationManager - Set the volume controls visible or not
 */
void dbus_set_volume_controls(  );

/**
 * ConfigurationManager - Tells if the user wants to display the volume controls or not 
 * @return int 1 if the controls have to be displayed
 *	       0 otherwise
 */
int dbus_get_volume_controls( void );

/**
 * ConfigurationManager - Configure the start-up option
 * At startup, SFLphone can be displayed or start hidden in the system tray
 */
void dbus_start_hidden( void );

/**
 * ConfigurationManager - Gives the maximum number of calls the user wants to have in the history 
 * @return double The maximum number of calls
 */
guint dbus_get_max_calls( void );

/**
 * ConfigurationManager - Gives the maximum number of calls the user wants to have in the history 
 */
void dbus_set_max_calls( const guint calls );
/**
 * ConfigurationManager - Configure the start-up option
 * @return int	1 if SFLphone should start in the system tray
 *	        0 otherwise
 */
int dbus_is_start_hidden( void );

/**
 * ConfigurationManager - Configure the popup behaviour 
 * When SFLphone is in the system tray, you can configure when it popups
 * Never or only on incoming calls
 */
void dbus_switch_popup_mode( void );

/**
 * ConfigurationManager - Configure the popup behaviour
 * @return int	1 if it should popup on incoming calls
 *		0 if it should never popups  
 */
int dbus_popup_mode( void );

/**
 * ConfigurationManager - Configure the notification level
 * @return int	0 disable 
 *		1 enable
 */
guint dbus_get_notify( void );

/**
 * ConfigurationManager - Configure the notification level
 */
void dbus_set_notify( void );

/**
 * ConfigurationManager - Configure the mail notification level
 * @return int	0 disable 
 *		1 enable
 */
guint dbus_get_mail_notify( void );

/**
 * ConfigurationManager - Configure the mail notification level
 */
void dbus_set_mail_notify( void );

/**
 * ConfigurationManager - Start a tone when a new call is open and no numbers have been dialed 
 * @param start 1 to start
 *		0 to stop
 * @param type  TONE_WITH_MESSAGE
 *		TONE_WITHOUT_MESSAGE	
 */
void dbus_start_tone(const int start , const guint type); 

/** 
 * Instance - Send registration request to dbus service.
 * Manage the instances of clients connected to the server
 * @param pid The pid of the processus client
 * @param name The string description of the client. Here : GTK+ Client
 */
void dbus_register( int pid, gchar * name);

/**
 * Instance - Send unregistration request to dbus services
 * @param pid The pid of the processus
 */
void dbus_unregister(int pid);

#endif
