/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include <callable_obj.h>
#include <sflphone_const.h>
#include <time.h>

gint is_confID_confstruct ( gconstpointer a, gconstpointer b)
{
    conference_obj_t * c = (conference_obj_t*)a;
    if(g_strcasecmp(c->_confID, (const gchar*) b) == 0)
    {
        return 0;
    }
    else
    {
        return 1;
    }
}

conference_obj_t* create_new_conference (conference_state_t state, const gchar* confID, conference_obj_t ** conf)
{
    DEBUG("create_new_conference");

    // conference_obj_t *obj;
    conference_obj_t *new_conf;
    const gchar* conf_id;

    // Allocate memory
    new_conf = g_new0 (conference_obj_t, 1);

    // Set state field    
    new_conf->_state = state;

    // Set the ID field
    conf_id = confID;
    new_conf->_confID = g_strdup (conf_id);

    new_conf->participant_list = NULL;

    *conf = new_conf;

}

conference_obj_t* create_new_conference_from_details (const gchar *conf_id, GHashTable *details, conference_obj_t ** conf)
{    
    DEBUG("create_new_conference_from_details");

    conference_obj_t *new_conf;
    gchar* call_id;
    gchar** participants;
    gchar** part;
    gchar* state_str;
    // GSList* participant_list;

    // Allocate memory
    new_conf = g_new0 (conference_obj_t, 1);

    new_conf->_confID = g_strdup (conf_id);

    new_conf->_conference_secured = FALSE;
    new_conf->_conf_srtp_enabled = FALSE;

    new_conf->participant_list = NULL;

    // get participant list
    participants = dbus_get_participant_list(conf_id);

    // generate conference participant list
    conference_participant_list_update(participants, new_conf);
 
    state_str = g_hash_table_lookup (details, "CONF_STATE");

    if (g_strcasecmp (state_str, "ACTIVE_ATACHED") == 0)
        new_conf->_state = CONFERENCE_STATE_ACTIVE_ATACHED;

    else if (g_strcasecmp (state_str, "ACTIVE_DETACHED") == 0)
        new_conf->_state = CONFERENCE_STATE_ACTIVE_DETACHED;

    else if (g_strcasecmp (state_str, "HOLD") == 0)
        new_conf->_state = CONFERENCE_STATE_HOLD;

    *conf = new_conf;
}

void free_conference_obj_t (conference_obj_t *c)
{
    g_free (c->_confID);

    if(c->participant_list)
        g_slist_free (c->participant_list);

    g_free (c);
}


void conference_add_participant(const gchar* call_id, conference_obj_t* conf)
{
    // store the new participant list after appending participant id
    conf->participant_list = g_slist_append(conf->participant_list, (gpointer)call_id);
}


void conference_remove_participant(const gchar* call_id, conference_obj_t* conf)
{
    // store the new participant list after removing participant id
    conf->participant_list = g_slist_remove(conf->participant_list, (gconstpointer)call_id);
}


GSList* conference_next_participant(GSList* participant)
{
    return g_slist_next(participant);
}


GSList* conference_participant_list_update(gchar** participants, conference_obj_t* conf)
{
    gchar* call_id;
    gchar** part;

    if(conf->participant_list) {
        g_slist_free(conf->participant_list);
	conf->participant_list = NULL;
    }

    DEBUG("Conference: Participant list update");

    for (part = participants; *part; part++) {
        call_id = (gchar*)(*part);
	DEBUG("Adding %s", call_id);
	conference_add_participant(call_id, conf);
    }

}
