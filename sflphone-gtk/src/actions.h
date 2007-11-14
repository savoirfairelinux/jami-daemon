/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc@squidy.info>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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

#include <calllist.h>
#include <accountlist.h>

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

void sflphone_on_hold ();
void sflphone_off_hold ();
call_t * sflphone_new_call();
void sflphone_notify_voice_mail (guint count);
void sflphone_set_transfert();
void sflphone_unset_transfert();
/**
 * Accept / dial the current call
 */
void sflphone_pick_up ();

/**
 * Put the call on hold state
 */
void sflphone_hold ( call_t * c);

/**
 * Put the call in Ringing state
 */
void sflphone_ringing(call_t * c );

void sflphone_busy( call_t * c );
void sflphone_fail( call_t * c );

/**
 * Put the call in Current state
 */
void sflphone_current ( call_t * c);

/**
 * The callee has hung up 
 */
void sflphone_hung_up( call_t * c);

/**
 * Incoming call
 */
void sflphone_incoming_call ( call_t * c);

/**
 * Dial the number
 * If the call is in DIALING state, the char will be append to the number
 * @TODO If the call is in CURRENT state, the char will be also sent to the server 
 * @param keyval The unique int representing the key
 * @param keyval The char value of the key
 */
void sflphone_keypad ( guint keyval, gchar * key);

/**
 * Place a call with a filled call_t.to 
 * @param c A call in CALL_STATE_DIALING state
 */
void sflphone_place_call ( call_t * c );

void sflphone_fill_account_list();
void sflphone_set_default_account();
#endif 
