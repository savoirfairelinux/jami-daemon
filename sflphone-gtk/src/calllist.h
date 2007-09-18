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
 
#ifndef __CALLLIST_H__
#define __CALLLIST_H__

#include <gtk/gtk.h>

typedef enum
{
   CALL_STATE_INVALID = 0,
   CALL_STATE_INCOMING, /* Ringing incoming call */
   CALL_STATE_RINGING,  /* Ringing outgoing call */
   CALL_STATE_CURRENT,
   CALL_STATE_DIALING,
   CALL_STATE_HOLD   
} call_state_t;

typedef struct  {
  gchar * callID;
  gchar * accountID;
  gchar * from;
  gchar * to;
  call_state_t state;
} call_t;

void call_list_init ();

void call_list_clean ();

void call_list_add (call_t * c);

void call_list_remove (const gchar * callID);

call_t * call_list_get_by_state ( call_state_t state);

guint call_list_get_size ( );

call_t * call_list_get_nth ( guint n );
call_t * call_list_get ( const gchar * callID );

gchar * call_get_name (const call_t * c);

gchar * call_get_number (const call_t * c);

#endif 
