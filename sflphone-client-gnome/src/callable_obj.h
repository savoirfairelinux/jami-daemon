/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
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

#ifndef __CALLABLE_OBJ_H__
#define __CALLABLE_OBJ_H__

#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <time.h>

/**
 * @enum history_state
 * This enum have all the state a call can take in the history
 */
typedef enum
{
  MISSED,
  INCOMING,
  OUTGOING
} history_state_t;

/**
 * @enum contact_type
 * This enum have all types of contacts: HOME phone, cell phone, etc...
 */
typedef enum
{
  CONTACT_PHONE_HOME,
  CONTACT_PHONE_BUSINESS,
  CONTACT_PHONE_MOBILE
} contact_type_t;

/**
 * @enum callable_obj_type
 * This enum have all types of items
 */
typedef enum
{
  CALL,
  HISTORY_ENTRY,
  CONTACT
} callable_type_t;


/** @enum call_state_t
  * This enum have all the states a call can take.
  */
typedef enum
{
   CALL_STATE_INVALID = 0,
   CALL_STATE_INCOMING,
   CALL_STATE_RINGING,
   CALL_STATE_CURRENT,
   CALL_STATE_DIALING,
   CALL_STATE_HOLD,
   CALL_STATE_FAILURE,
   CALL_STATE_BUSY,
   CALL_STATE_TRANSFERT,
   CALL_STATE_RECORD
} call_state_t;


/** @struct callable_obj_t
  * @brief Call information.
  * This struct holds information about a call.
  */
typedef struct  {

    callable_type_t _type;          // CALL - HISTORY ENTRY - CONTACT
    call_state_t _state;             // The state of the call
    gchar* _callID;                 // The call ID
    gchar* _accountID;              // The account the call is made with
    time_t _time_start;              // The timestamp the call was initiating
    time_t _time_stop;              // The timestamp the call was over
    history_state_t _history_state;  // The history state if necessary

    /**
     * The information about the person we are talking
     */
    gchar *_peer_name;
    gchar *_peer_number;

    /**
     * Used to contain the transfer information
     */
    gchar *_trsft_to;

    /**
      * A well-formatted phone information
      */
    gchar *_peer_info;

    /**
     * The thumbnail, if callable_obj_type=CONTACT
     */
    GdkPixbuf *_contact_thumbnail;

} callable_obj_t;

void create_new_call (callable_type_t, call_state_t, gchar*, gchar*, gchar*, gchar*, callable_obj_t **);

void create_new_call_from_details (const gchar *, GHashTable *, callable_obj_t **);

void create_history_entry_from_serialized_form (gchar *, gchar *, callable_obj_t **);

/* 
 * GCompareFunc to compare a callID (gchar* and a callable_obj_t) 
 */
gint is_callID_callstruct ( gconstpointer, gconstpointer);

/* 
 * GCompareFunc to get current call (gchar* and a callable_obj_t) 
 */
gint get_state_callstruct ( gconstpointer, gconstpointer);

/** 
  * This function parse the callable_obj_t.from field to return the name
  * @param c The call
  * @return The full name of the caller or an empty string 
  */
gchar* call_get_peer_name (const gchar*);

/**
 * This function parse the callable_obj_t.from field to return the number
 * @param c The call
 * @return The number of the caller
 */
gchar* call_get_peer_number (const gchar*);




void
attach_thumbnail (callable_obj_t *, GdkPixbuf *);

void
free_callable_obj_t (callable_obj_t *c);

/**
 * @return gchar* A random ID
 */
gchar* generate_call_id (void);

gchar* get_peer_info (gchar*, gchar*);

history_state_t get_history_state_from_id (gchar *indice);

gchar* get_call_duration (callable_obj_t *obj);

gchar* serialize_history_entry (callable_obj_t *entry);

gchar* get_history_id_from_state (history_state_t state);

gchar* get_formatted_start_timestamp (callable_obj_t *obj);

void set_timestamp (time_t*);

gchar* convert_timestamp_to_gchar (time_t);

time_t convert_gchar_to_timestamp (gchar*);


#endif
