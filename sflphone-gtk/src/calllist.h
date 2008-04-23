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
 
#ifndef __CALLLIST_H__
#define __CALLLIST_H__

#include <gtk/gtk.h>
/** @file calllist.h
  * @brief A list to hold calls.
  */
  
/** @enum call_state_t 
  * This enum have all the states a call can take.
  */
typedef enum
{  /** Invalid state */
   CALL_STATE_INVALID = 0, 
   /** Ringing incoming call */
   CALL_STATE_INCOMING, 
   /** Ringing outgoing call */
   CALL_STATE_RINGING,  
   /** Call to which the user can speak and hear */
   CALL_STATE_CURRENT,  
   /** Call which numbers are being added by the user */
   CALL_STATE_DIALING,  
   /** Call is on hold */
   CALL_STATE_HOLD,      
   /** Call has failed */
   CALL_STATE_FAILURE,      
   /** Call is busy */
   CALL_STATE_BUSY,        
   /** Call is being transfert.  During this state, the user can enter the new number. */
   CALL_STATE_TRANSFERT       
} call_state_t;

/**
 * @enum history_state
 * This enum have all the state a call can take in the history
 */
typedef enum
{
  NONE,
  INCOMING,
  OUTGOING,
  MISSED
} history_state_t;

/** @struct call_t
  * @brief Call information.
  * This struct holds information about a call.    
  */
typedef struct  {
  /** Unique identifier of the call */
  gchar * callID;
  /** The account used to place/receive the call */
  gchar * accountID;
/** The information about the calling person.  See call_get_name() and call_get_number()
    * on how to get the name and number separately. */
  gchar * from;
  /** The number we are calling.  Only used when dialing out */
  gchar * to;
  /** The current state of the call */
  call_state_t state;
  /** The history state */
  history_state_t history_state;
} call_t;

typedef struct {
	GtkListStore* store;
	GtkWidget* view;
	GtkWidget* tree;

	// Calllist vars
	GQueue* callQueue;
	call_t* selectedCall;
} calltab_t;

calltab_t* current_calls;
calltab_t* history;

/** This function initialize a call list. */
void call_list_init (calltab_t* tab);

/** This function empty and free the call list. */
void call_list_clean (calltab_t* tab);

/** Get the maximun number of calls in the history calltab */
gdouble call_history_get_max_calls( void ); 

/** Set the maximun number of calls in the history calltab */
void call_history_set_max_calls( const gdouble number ); 

/** This function append a call to list. 
  * @param c The call you want to add 
  * */
void call_list_add (calltab_t* tab, call_t * c);

/** This function remove a call from list. 
  * @param callID The callID of the call you want to remove
  */
void call_list_remove (calltab_t* tab, const gchar * callID);

/** Return the first call that corresponds to the state.  
  * This is usefull for unique states as DIALING and CURRENT.
  * @param state The state
  * @return A call or NULL */
call_t * call_list_get_by_state (calltab_t* tab, call_state_t state);

/** Return the number of calls in the list
  * @return The number of calls in the list */
guint call_list_get_size (calltab_t* tab);

/** Return the call at the nth position in the list
  * @param n The position of the call you want
  * @return A call or NULL */
call_t * call_list_get_nth (calltab_t* tab, guint n );

/** Return the call corresponding to the callID
  * @param n The callID of the call you want
  * @return A call or NULL */
call_t * call_list_get (calltab_t* tab, const gchar * callID );

/** This function parse the call_t.from field to return the name
  * @param c The call
  * @return The full name of the caller or an empty string */
gchar * call_get_name ( const call_t * c);

/** 
 * This function parse the call_t.from field to return the number
 * @param c The call
 * @return The number of the caller 
 */
gchar * call_get_number (const call_t * c);

/** Mark a call as selected.  There can be only one selected call.  This call
  * is the currently highlighted one in the list.
  * @param c The call */
void call_select (calltab_t* tab, call_t * c );

/** Return the selected call.
  * @return The number of the caller */
call_t * call_get_selected (calltab_t* tab);
#endif 
