/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
#include "dbus.h"
#include "sflphone_const.h"
#include "logger.h"
#include "calltab.h"
#include "calllist.h"

conference_obj_t *create_new_conference(conference_state_t state, const gchar* const confID)
{
    if (confID == NULL) {
        ERROR("Conference: Error: Conference ID is NULL while creating new conference");
        return NULL;
    }

    DEBUG("Conference: Create new conference %s", confID);

    // Allocate memory
    conference_obj_t *new_conf = g_new0(conference_obj_t, 1);

    if (!new_conf) {
        ERROR("Conference: Error: Could not allocate data ");
        return NULL;
    }

    // Set state field
    new_conf->_state = state;

    // Set the ID field
    new_conf->_confID = g_strdup(confID);

    // set conference timestamp
    time(&new_conf->_time_start);

    return new_conf;
}

conference_obj_t *create_new_conference_from_details(const gchar *conf_id, GHashTable *details)
{
    conference_obj_t *new_conf = g_new0(conference_obj_t, 1);
    new_conf->_confID = g_strdup(conf_id);

    gchar **participants = dbus_get_participant_list(conf_id);

    if (participants) {
        conference_participant_list_update(participants, new_conf);
        g_strfreev(participants);
    }

    gchar *state_str = g_hash_table_lookup(details, "CONF_STATE");

    if (g_strcasecmp(state_str, "ACTIVE_ATTACHED") == 0)
        new_conf->_state = CONFERENCE_STATE_ACTIVE_ATTACHED;
    else if (g_strcasecmp(state_str, "ACTIVE_ATTACHED_REC") == 0)
        new_conf->_state = CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD;
    else if (g_strcasecmp(state_str, "ACTIVE_DETACHED") == 0)
        new_conf->_state = CONFERENCE_STATE_ACTIVE_DETACHED;
    else if (g_strcasecmp(state_str, "ACTIVE_DETACHED_REC") == 0)
        new_conf->_state = CONFERENCE_STATE_ACTIVE_DETACHED_RECORD;
    else if (g_strcasecmp(state_str, "HOLD") == 0)
        new_conf->_state = CONFERENCE_STATE_HOLD;
    else if (g_strcasecmp(state_str, "HOLD_REC") == 0)
        new_conf->_state = CONFERENCE_STATE_HOLD_RECORD;

    return new_conf;
}


void free_conference_obj_t (conference_obj_t *c)
{
    g_free(c->_confID);

    if (c->participant_list)
        g_slist_free(c->participant_list);

    g_free(c);
}

static
void conference_add_participant_number(const gchar *call_id, conference_obj_t *conf)
{
    callable_obj_t *call = calllist_get_call(current_calls, call_id);

    if (!call) {
        ERROR("Conference: Error: Could not find %s", call_id);
        return;
    }

    gchar *number_account = g_strconcat(call->_peer_number, ",", call->_accountID, NULL);
    conf->participant_number = g_slist_append(conf->participant_number, number_account);
}

void conference_add_participant(const gchar* call_id, conference_obj_t* conf)
{
    DEBUG("Conference: Conference %s, adding participant %s", conf->_confID, call_id);

    // store the new participant list after appending participant id
    conf->participant_list = g_slist_append(conf->participant_list, (gpointer) g_strdup(call_id));

    // store the phone number of this participant
    conference_add_participant_number(call_id, conf);
}

void conference_remove_participant(const gchar* call_id, conference_obj_t* conf)
{
    // store the new participant list after removing participant id
    conf->participant_list = g_slist_remove(conf->participant_list, (gconstpointer) call_id);
}


GSList* conference_next_participant(GSList* participant)
{
    return g_slist_next(participant);
}


void conference_participant_list_update(gchar** participants, conference_obj_t* conf)
{
    DEBUG("Conference: Participant list update");

    if (!conf) {
        ERROR("Conference: Error: Conference is NULL");
        return;
    }

    for (gchar **part = participants; part && *part; part++) {
        gchar *call_id = (gchar *)(*part);
        callable_obj_t *call = calllist_get_call(current_calls, call_id);

        if (call->_confID != NULL) {
            g_free(call->_confID);
            call->_confID = NULL;
        }
    }

    if (conf->participant_list) {
        g_slist_free(conf->participant_list);
        conf->participant_list = NULL;
    }

    for (gchar **part = participants; part && *part; part++) {
        gchar *call_id = (gchar*)(*part);
        callable_obj_t *call = calllist_get_call(current_calls, call_id);
        call->_confID = g_strdup(conf->_confID);
        conference_add_participant(call_id, conf);
    }
}

gchar *serialize_history_conference_entry(conference_obj_t *entry)
{
    gchar *result = "";
    static const gchar * const separator = "|";
    gchar *time_start = "";
    gchar *time_stop = "";
    gchar *peer_name = "";
    gchar *participantstr = "";
    gchar *confID = "";
    GSList *participant_list;
    gint length = 0;
    gint i;

    confID = entry->_confID;

    time_start = g_strdup_printf("%i", (int) entry->_time_start);
    time_stop = g_strdup_printf("%i", (int) entry->_time_stop);

    peer_name = (entry->_confID == NULL || (strlen(entry->_confID) == 0)) ? "empty": entry->_confID;

    length = g_slist_length(entry->participant_list);
    participant_list = entry->participant_list;

    for (i = 0; i < length; i++) {
        const gchar * const tmp = g_slist_nth_data(participant_list, i);

        if (!tmp)
            WARN("Conference: Peer number is NULL in conference list");

        participantstr = g_strconcat(participantstr, tmp, ";", NULL);

        DEBUG("Conference: Participant number: %s, concatenation: %s", tmp, participantstr);
    }

    result = g_strconcat("9999", separator,
                         participantstr, separator, // peer number
                         peer_name, separator,
                         time_start, separator,
                         time_stop, separator,
                         confID, separator,
                         "empty", separator, // peer AccountID
                         entry->_recordfile ? entry->_recordfile : "", separator,
                         "empty", separator,
                         "empty", NULL);

    return result;
}

conference_obj_t *create_conference_history_entry_from_serialized(gchar *entry)
{
    history_state_t history_state = MISSED;
    conference_state_t state = CONFERENCE_STATE_ACTIVE_ATTACHED;
    const gchar *participant = NULL;
    const gchar *name = NULL;
    const gchar *time_start = NULL;
    const gchar *time_stop = NULL;
    const gchar *accountID = NULL;
    const gchar *recordfile = NULL;
    const gchar *confID = NULL;

    DEBUG("Conference: Create a conference from serialized form");

    static const gchar * const delim = "|";
    gchar **ptr = g_strsplit(entry, delim, 10);
    gint token = 0;

    while (ptr && token < 10) {
        switch (token) {
            case 0:
                history_state = MISSED;
                break;
            case 1:
                participant = *ptr;
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
            case 8:
            case 9:
            default:
                break;
        }

        token++;
        ptr++;
    }

    // create a new empty conference
    conference_obj_t *conf = create_new_conference(state, confID);

    conf->_time_start = atoi(time_start);
    conf->_time_stop = atoi(time_stop);
    conf->_recordfile = g_strdup(recordfile);

    return conf;
}
