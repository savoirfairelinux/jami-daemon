/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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

void sflphone_hang_up ( call_t * c);

void sflphone_transfert ( call_t * c, gchar * to );

void sflphone_hold ( call_t * c);

void sflphone_ringing(call_t * c );

void sflphone_unhold ( call_t * c);

/* signals */
void sflphone_hung_up( call_t * c);

/* void sflphone_ring */
void sflphone_incoming_call ( call_t * c);

/**
 * Dial the number
 * If the call is in DIALING state, the char will be append to the number
 * TODO If the call is in CURRENT state, the char will be also sent to the server 
 * @param c A call in CALL_STATE_DIALING state
 */
void sflphone_keypad ( guint keyval, gchar * key);

/**
 * Place a call
 * @param c A call in CALL_STATE_DIALING state
 */
void sflphone_place_call ( call_t * c );

#endif 
