/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#ifndef __ACTIONS_H__
#define __ACTIONS_H__

#include <libintl.h>
#include <locale.h>

#include <accountlist.h>
#include <codeclist.h>
#include <sflphone_const.h>
#include <errors.h>
#include <conference_obj.h>

#define	UC(b)	(((int)b)&0xff)

/** @file actions.h
  * @brief General functions that change the state of the application.
  * All of these functions are called when dbus signals are triggered.  Exceptions
  * are sflphone_init() sflphone_quit(), sflphone_keypad() and sflphone_place_call().
  */


/**
 * Initialize lists and configurations
 * @return TRUE if succeeded, FALSE otherwise
 */
gboolean sflphone_init ( ) ;

/**
 * Steps when closing the application.  Will ask for confirmation if a call is in progress.
 * @return TRUE if the user wants to quit, FALSE otherwise.
 */
gboolean sflphone_quit ( ) ;

/**
 * Hang up / refuse the current call
 */
void sflphone_hang_up ();

/**
 * Put the selected call on hold
 */
void sflphone_on_hold ();

/**
 * Put the selected call off hold
 */
void sflphone_off_hold ();

/**
 * Open a new call
 * @return callable_obj_t* A pointer on the call structure
 */
callable_obj_t * sflphone_new_call();

/**
 * Notify voice mails to the application
 * @param accountID The account the voice mails are for
 * @param count The number of voice mails
 */
void sflphone_notify_voice_mail ( const gchar* accountID , guint count );

/**
 * Prepare SFLphone to transfer a call and wait for the user to dial the number to transfer to
 * Put the selected call in Transfer state
 */
void sflphone_set_transfert();

/**
 * Cancel the transfer and puts back the selected call to Current state
 */
void sflphone_unset_transfert();

/**
 * Accept / dial the current call
 */
void sflphone_pick_up ();

/**
 * Put the call on hold state
 * @param c The current call
 */
void sflphone_hold ( callable_obj_t * c);

/**
 * Put the call in Ringing state
 * @param c* The current call
 */
void sflphone_ringing(callable_obj_t * c );

/**
 * Put the call in Busy state
 * @param c* The current call
 */
void sflphone_busy( callable_obj_t * c );

/**
 * Put the call in Failure state
 * @param c* The current call
 */
void sflphone_fail( callable_obj_t * c );

/**
 * Put the call in Current state
 * @param c The current call
 */
void sflphone_current ( callable_obj_t * c);

/**
 * The callee has hung up
 * @param c The current call
 */
void sflphone_hung_up( callable_obj_t * c);

/**
 * Incoming call
 * @param c The incoming call
 */
void sflphone_incoming_call ( callable_obj_t * c);

/**
 * Dial the number
 * If the call is in DIALING state, the char will be append to the number
 * @param keyval The unique int representing the key
 * @param key The char value of the key
 */
void sflphone_keypad ( guint keyval, gchar * key);

/**
 * Place a call with a filled callable_obj_t.to
 * @param c A call in CALL_STATE_DIALING state
 */
void sflphone_place_call ( callable_obj_t * c );

/**
 * Fetch the ip2ip profile through dbus and fill
 * the internal hash table. 
 */
void sflphone_fill_ip2ip_profile(void);

/**
 * @return The internal hash table representing
 * the settings for the ip2ip profile. 
 */
void sflphone_get_ip2ip_properties (GHashTable **properties);
 
/**
 * Initialize the accounts data structure
 */
void sflphone_fill_account_list ();

void sflphone_fill_call_list (void);

/**
 * Set an account as current. The current account is to one used to place calls with by default
 * The current account is the first in the account list ( index 0 )
 */
void sflphone_set_current_account();

/**
 * Initialialize the codecs data structure
 */
void sflphone_fill_codec_list ();

void sflphone_fill_codec_list_per_account (account_t **);


void sflphone_add_participant();

void sflphone_record (callable_obj_t *c);

void sflphone_rec_call (void);

void status_bar_display_account ();

void sflphone_fill_history (void);

void sflphone_save_history (void);

/**
 * Action called when two single call are dragged on together to create a new conference
 */
void sflphone_join_participant(const gchar* sel_callID, const gchar* drag_callID);

/**
 * Action called when a new participant is dragged in
 */
void sflphone_add_participant(const gchar* callID, const gchar* confID);

/**
 * Action called when a conference participant is draged out
 */
void sflphone_detach_participant(const gchar* callID);

/**
 * Action called when two conference are merged together
 */
void sflphone_join_conference(const gchar* sel_confID, const gchar* drag_confID);


/** 
 * Nofity that the communication is 
 * now secured using SRTP/SDES.
 * @param c* The current call
 */
void sflphone_srtp_sdes_on(callable_obj_t * c);

/** 
 * Notify that the SRTP/SDES session
 * is not secured
 */

/** 
 * Nofity that the communication is 
 * now secured using ZRTP.
 * @param c* The current call
 */
void sflphone_srtp_zrtp_on( callable_obj_t * c);

/** 
 * Called when the ZRTP session goes
 * unsecured.
 * @param c* The current call
 */
void sflphone_srtp_zrtp_off( callable_obj_t * c );

/** 
 * Called when the sas has been computed
 * and is ready to be displayed.
 * @param c* The current call
 * @param sas* The Short Authentication String
 * @param verified* Weather the SAS was confirmed or not.
 */
void sflphone_srtp_zrtp_show_sas( callable_obj_t * c, const gchar* sas, const gboolean verified);

/** 
 * Called when the remote peer does not support ZRTP
 * @param c* The current call
 */
void sflphone_srtp_zrtp_not_supported( callable_obj_t * c );

/** 
 * Called when user wants to confirm go clear request.
 * @param c* The call to confirm the go clear request.
 */
void sflphone_set_confirm_go_clear( callable_obj_t * c );

/** 
 * Called when user wants to confirm go clear request.
 * @param c* The call to confirm the go clear request.
 */
void sflphone_confirm_go_clear( callable_obj_t * c );

/**
 * Called when user wants to clear.
 * @param c* The call on which to go clear
 */
void sflphone_request_go_clear(void);

/** 
 * Called when the UI needs to be refreshed to 
 * better inform the user about the current
 * state of the call. 
 * @param c A pointer to the call that needs to be updated
 * @param description A textual description of the code
 * @param code The status code as in SIP or IAX
 */
void sflphone_call_state_changed(callable_obj_t * c, const gchar * description, const guint code);

/**
 * Resolve an interface address given its name
 */
void sflphone_get_interface_addr_from_name(char *iface_name, char **iface_addr, int size);
#endif
