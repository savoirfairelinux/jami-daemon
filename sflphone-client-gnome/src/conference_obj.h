/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#ifndef __CONFERENCE_OBJ_H__
#define __CONFERENCE_OBJ_H__

#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <time.h>



/** @enum conference_state_t
  * This enum have all the states a conference can take.
  */
typedef enum {
    CONFERENCE_STATE_ACTIVE_ATACHED = 0,
    CONFERENCE_STATE_ACTIVE_DETACHED,
    CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD,
    CONFERENCE_STATE_ACTIVE_DETACHED_RECORD,
    CONFERENCE_STATE_HOLD,
    CONFERENCE_STATE_HOLD_RECORD
} conference_state_t;


/** @struct conference_obj_t
  * @brief Conference information.
  * This struct holds information about a conference.
  */
typedef struct  {
    conference_state_t _state;       // The state of the call
    gchar *_confID;                  // The call ID
    gboolean _conference_secured;    // the security state of the conference
    gboolean _conf_srtp_enabled;     // security required for this conference
    GSList *participant_list;        // participant list for this
    GSList *participant_number;
    GtkWidget *_im_widget;           // associated instant messaging widget
    time_t _time_start;
    time_t _time_stop;
    time_t _time_current;
    gchar *_recordfile;
    gboolean _record_is_playing;
} conference_obj_t;

void create_new_conference (conference_state_t, const gchar*, conference_obj_t **);

void create_new_conference_from_details (const gchar *, GHashTable *, conference_obj_t **);

void free_conference_obj_t (conference_obj_t *c);

void conference_add_participant (const gchar*, conference_obj_t *);

void conference_remove_participant (const gchar*, conference_obj_t *);

GSList* conference_next_participant (GSList* participant);

void conference_participant_list_update (gchar**, conference_obj_t*);

gchar *serialize_history_conference_entry(conference_obj_t *entry);

void create_conference_history_entry_from_serialized(gchar *, conference_obj_t **);

#endif
