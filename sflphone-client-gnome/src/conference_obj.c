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

#include <time.h>

#include "callable_obj.h"
#include "sflphone_const.h"

#include "calltab.h"
#include "calllist.h"

static void set_conference_timestamp (time_t *);
static void conference_add_participant_number(const gchar *, conference_obj_t *);
static void process_conference_participant_from_serialized(gchar *, conference_obj_t *);

static void set_conference_timestamp (time_t *timestamp) 
{
    time_t tmp;

    // Set to current value
    (void) time(&tmp);
    *timestamp = tmp;
}

void create_new_conference (conference_state_t state, const gchar* confID, conference_obj_t ** conf)
{
    conference_obj_t *new_conf;

    if(confID == NULL) {
	ERROR("Conference: Error: Conference ID is NULL while creating new conference");
	return;
    }

    DEBUG ("Conference: Create new conference %s", confID);

    // Allocate memory
    new_conf = g_new0 (conference_obj_t, 1);
    if(new_conf == NULL) {
	ERROR("Conference: Error: Could not allocate data ");
	return;
    }

    // Set state field
    new_conf->_state = state;

    // Set the ID field
    new_conf->_confID = g_strdup (confID);

    new_conf->participant_list = NULL;
    new_conf->participant_number = NULL;

    new_conf->_recordfile = NULL;
    new_conf->_record_is_playing = FALSE;

    set_conference_timestamp(&new_conf->_time_start);

    *conf = new_conf;
}

void create_new_conference_from_details (const gchar *conf_id, GHashTable *details, conference_obj_t ** conf)
{
    conference_obj_t *new_conf;
    gchar** participants;
    gchar* state_str;
    
    DEBUG ("Conference: Create new conference from details");

    // Allocate memory
    new_conf = g_new0 (conference_obj_t, 1);
    if(new_conf == NULL) {
        ERROR("Conference: Error: Could not allocate data ");
        return;
    }

    new_conf->_confID = g_strdup (conf_id);

    new_conf->_conference_secured = FALSE;
    new_conf->_conf_srtp_enabled = FALSE;

    new_conf->participant_list = NULL;

    // get participant list
    participants = dbus_get_participant_list (conf_id);
    if(participants == NULL) {
	ERROR("Conference: Error: Could not get participant list");
    }

    // generate conference participant list
    conference_participant_list_update (participants, new_conf);

    state_str = g_hash_table_lookup (details, "CONF_STATE");

    if (g_strcasecmp (state_str, "ACTIVE_ATACHED") == 0) {
        new_conf->_state = CONFERENCE_STATE_ACTIVE_ATACHED;
    } else if (g_strcasecmp (state_str, "ACTIVE_ATTACHED_REC") == 0) {
        new_conf->_state = CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD;
    } else if (g_strcasecmp (state_str, "ACTIVE_DETACHED") == 0) {
        new_conf->_state = CONFERENCE_STATE_ACTIVE_DETACHED;
    } else if (g_strcasecmp (state_str, "ACTIVE_DETACHED_REC") == 0) {
        new_conf->_state = CONFERENCE_STATE_ACTIVE_DETACHED_RECORD;
    } else if (g_strcasecmp (state_str, "HOLD") == 0) {
        new_conf->_state = CONFERENCE_STATE_HOLD;
    } else if (g_strcasecmp (state_str, "HOLD_REC") == 0) {
        new_conf->_state = CONFERENCE_STATE_HOLD_RECORD;
    }

    new_conf->_recordfile = NULL;
    new_conf->_record_is_playing = FALSE;

    *conf = new_conf;
}


void free_conference_obj_t (conference_obj_t *c)
{
    g_free (c->_confID);

    if (c->participant_list) {
        g_slist_free(c->participant_list);
    }

    g_free (c);
}


void conference_add_participant (const gchar* call_id, conference_obj_t* conf)
{
    DEBUG("Conference: Conference %s, adding participant %s", conf->_confID, call_id);

    // store the new participant list after appending participant id
    conf->participant_list = g_slist_append (conf->participant_list, (gpointer) g_strdup(call_id));

    // store the phone number of this participant
    conference_add_participant_number(call_id, conf);
}

static
void conference_add_participant_number(const gchar *call_id, conference_obj_t *conf)
{
    gchar *number_account;

    callable_obj_t *call = calllist_get_call(current_calls, call_id); 
    if(call == NULL) {
	ERROR("Conference: Error: Could not find");
	return;
    }
    
    number_account = g_strconcat(call->_peer_number, ",", call->_accountID, NULL);

    conf->participant_number = g_slist_append(conf->participant_number, (gpointer) number_account);
}

void conference_remove_participant (const gchar* call_id, conference_obj_t* conf)
{
    // store the new participant list after removing participant id
    conf->participant_list = g_slist_remove (conf->participant_list, (gconstpointer) call_id);
}


GSList* conference_next_participant (GSList* participant)
{
    return g_slist_next (participant);
}


void conference_participant_list_update (gchar** participants, conference_obj_t* conf)
{
    gchar* call_id;
    gchar** part;
    callable_obj_t *call;

    DEBUG ("Conference: Participant list update");

    if(conf == NULL) {
    	ERROR("Conference: Error: Conference is NULL");
        return;
    }

    for(part = participants; *part; part++) {
	call_id = (gchar *) (*part);
	call = calllist_get_call(current_calls, call_id);
	if(call->_confID != NULL) {
	    g_free(call->_confID);
	    call->_confID = NULL;
	}
    }

    if (conf->participant_list) {
        g_slist_free (conf->participant_list);
        conf->participant_list = NULL;
    }

    for (part = participants; *part; part++) {
        call_id = (gchar*) (*part);
	call = calllist_get_call(current_calls, call_id);
	call->_confID = g_strdup(conf->_confID);
        conference_add_participant (call_id, conf);
    }

}

gchar *serialize_history_conference_entry(conference_obj_t *entry)
{
    gchar *result = "";
    gchar *separator = "|";
    gchar *time_start = "";
    gchar *time_stop = "";
    gchar *peer_name = "";
    gchar *participantstr = "";
    gchar *confID = "";
    GSList *participant_list;
    gint length = 0;
    gint i;

    confID = entry->_confID;

    time_start = convert_timestamp_to_gchar(entry->_time_start);
    time_stop = convert_timestamp_to_gchar(entry->_time_stop);
 
    peer_name = (entry->_confID == NULL || g_strcasecmp(entry->_confID, "") == 0) ? "empty": entry->_confID;

    length = g_slist_length(entry->participant_list);
    participant_list = entry->participant_list;

    for(i = 0; i < length; i++) {
	gchar *tmp = g_slist_nth_data(participant_list, i);
	if(tmp == NULL) {
            WARN("Conference: Peer number is NULL in conference list");
        }
        participantstr = g_strconcat(participantstr, tmp, ";", NULL);
	

	DEBUG("Conference: Participant number: %s, concatenation: %s", tmp, participantstr);
    }

    result = g_strconcat("9999", separator,
			participantstr, separator, // peer number
			peer_name, separator,
			time_start, separator,
			time_stop, separator,
			confID, separator,
			"", separator, // peer AccountID
			entry->_recordfile ? entry->_recordfile : "",
			NULL); 
  	

    return result;
}

void create_conference_history_entry_from_serialized(gchar *entry, conference_obj_t **conf)
{
    history_state_t history_state = MISSED;
    gint token = 0;
    conference_state_t state = CONFERENCE_STATE_ACTIVE_ATACHED;
    gchar *participant = "";
    gchar *name = "";
    gchar *time_start = "";
    gchar *time_stop = "";
    gchar *accountID = "";
    gchar *recordfile = "";
    const gchar *confID = "";
    gchar **ptr;
    gchar *delim = "|";
    
    DEBUG("Conference: Create a conference from serialized form");
 
    ptr = g_strsplit(entry, delim, 8);
    while(ptr != NULL && token < 8) {
        switch(token) {
            case 0:
		history_state = MISSED;
		break;
	    case 1:
		participant = g_strdup(*ptr);
		break;
	    case 2:
		name = *ptr;
		break;
	    case 3:
		time_start = *ptr;
		break;
	    case 4:
		time_stop = *ptr;
		break;
 	    case 5:
		confID = *ptr;
		break;
	    case 6:
		accountID = *ptr;
		break;
	    case 7:
	        recordfile = *ptr;
		break;
	    default:
	        break;
	}

	token++;
 	ptr++;
    }

    // create a new empty conference
    create_new_conference(state, confID, conf);
    
    // process_conference_participant_from_serialized(participant, *conf);

    g_free(participant);
}

static void process_conference_participant_from_serialized(gchar *participant, conference_obj_t *conf)
{
    gchar **ptr = NULL;
    gchar *delim = ";";
    gint tok = 0;
    

    DEBUG("Conference: Process participant from serialized form");

    ptr = g_strsplit(participant, delim, 3);
    while(ptr != NULL && (tok < 2)) {
	gchar *call_id = NULL;

	conference_add_participant(call_id, conf);
	
	tok++;
	ptr++;
    }
} 
