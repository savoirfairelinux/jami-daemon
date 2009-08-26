/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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

void create_new_conference (conference_state_t state, gchar* confID, conference_obj_t ** new_conf)
{

    conference_obj_t *obj;
    gchar *conf_id;

    // Allocate memory
    obj = g_new0 (conference_obj_t, 1);

    // Set state field    
    obj->_state = state;

    // Set the ID field
    conf_id = confID;
    obj->_confID = g_strdup (conf_id);
    *new_conf = obj;
    
}

void create_new_conference_from_details (const gchar *conf_id, GHashTable *details, conference_obj_t *conf)
{
    /*
    gchar *peer_name, *peer_number, *accountID, *state_str;
    callable_obj_t *new_call;
    call_state_t state;

    accountID = g_hash_table_lookup (details, "ACCOUNTID");
    peer_number = g_hash_table_lookup (details, "PEER_NUMBER");
    peer_name = g_strdup ("");
    state_str = g_hash_table_lookup (details, "CALL_STATE");


    if (g_strcasecmp (state_str, "CURRENT") == 0)
        state = CALL_STATE_CURRENT;

	else if (g_strcasecmp (state_str, "RINGING") == 0)
		state = CALL_STATE_RINGING;

	else if (g_strcasecmp (state_str, "INCOMING") == 0)
		state = CALL_STATE_INCOMING;

    else if (g_strcasecmp (state_str, "HOLD") == 0)
        state = CALL_STATE_HOLD;

    else if (g_strcasecmp (state_str, "BUSY") == 0)
        state = CALL_STATE_BUSY;

    else
        state = CALL_STATE_FAILURE;

    create_new_call (CALL, state, (gchar*)call_id, accountID, peer_name, peer_number, &new_call);
    *call = new_call;
    */
}

void free_conference_obj_t (conference_obj_t *c)
{
    g_free (c->_confID);
    g_free (c);
}
