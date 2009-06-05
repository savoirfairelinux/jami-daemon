/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
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
 */

#ifndef __ACTIONS_H__
#define __ACTIONS_H__

#include <libintl.h>
#include <locale.h>

#include <accountlist.h>
#include <codeclist.h>
#include <sflphone_const.h>
#include <errors.h>

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
 * Initialize the accounts data structure
 */
void sflphone_fill_account_list(gboolean toolbarInitialized);

void sflphone_fill_call_list (void);

/**
 * Set an account as current. The current account is to one used to place calls with by default
 * The current account is the first in the account list ( index 0 )
 */
void sflphone_set_current_account();

/**
 * Initialialize the codecs data structure
 */
void sflphone_fill_codec_list();

void sflphone_record (callable_obj_t *c);

void sflphone_rec_call (void);

gchar* sflphone_get_current_codec_name();

void sflphone_display_selected_codec (const gchar* codecName);

void status_bar_display_account ();

void sflphone_fill_history (void);
#endif
