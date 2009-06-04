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

#ifndef __CALLL_H__
#define __CALLL_H__

#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <stdlib.h>

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
 * @enum call_type
 * This enum have all types of call
 */
typedef enum
{
  CALL,
  HISTORY,
  CONTACT
} call_type_t;


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
   CALL_STATE_TRANSFERT,
   /** Call is on hold */
   CALL_STATE_RECORD
} call_state_t;


/** @struct call_t
  * @brief Call information.
  * This struct holds information about a call.
  */
typedef struct  {
  /** Type of call entry */
  call_type_t call_type;
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
  /** The history state if necessary */
  history_state_t history_state;

  GdkPixbuf *contact_thumbnail;

  time_t _start;
  time_t _stop;

} call_t;


/* GCompareFunc to compare a callID (gchar* and a call_t) */
gint
is_callID_callstruct ( gconstpointer, gconstpointer);

/* GCompareFunc to get current call (gchar* and a call_t) */
gint
get_state_callstruct ( gconstpointer, gconstpointer);

/** This function parse the call_t.from field to return the name
  * @param c The call
  * @return The full name of the caller or an empty string */
gchar *
call_get_name (const call_t *);

/**
 * This function parse the call_t.from field to return the number
 * @param c The call
 * @return The number of the caller
 */
gchar *
call_get_number (const call_t *);

gchar *
call_get_recipient( const call_t *);

void
create_new_call (gchar *, gchar *, call_state_t, gchar *, call_t **);

void 
create_new_call_from_details (const gchar *, GHashTable *, call_t **);

void 
create_new_call_from_serialized_form (gchar *, gchar *, call_t **);

void
attach_thumbnail (call_t *, GdkPixbuf *);

void
free_call_t (call_t *c);

#endif
