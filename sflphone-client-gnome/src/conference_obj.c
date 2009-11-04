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

void create_new_conference (conference_state_t state, const gchar* confID, conference_obj_t ** conf)
{
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
    *conf = new_conf;

}

void create_new_conference_from_details (const gchar *conf_id, GHashTable *details, conference_obj_t *conf)
{    

    conference_obj_t *new_conf;
    // const gchar* conf_id;
    // gchar** participants;
    // gchar** part;
    gchar* state_str;

    // Allocate memory
    new_conf = g_new0 (conference_obj_t, 1);

    new_conf->_confID = g_strdup (conf_id);

    new_conf->_conference_secured = FALSE;
    new_conf->_conf_srtp_enabled = FALSE;

    new_conf->participant = dbus_get_participant_list(conf_id);
 
    state_str = g_hash_table_lookup (details, "CALL_STATE");

    if (g_strcasecmp (state_str, "ACTIVE_ATACHED") == 0)
        new_conf->_state = CONFERENCE_STATE_ACTIVE_ATACHED;

    else if (g_strcasecmp (state_str, "ACTIVE_DETACHED") == 0)
        new_conf->_state = CONFERENCE_STATE_ACTIVE_DETACHED;

    else if (g_strcasecmp (state_str, "HOLD") == 0)
        new_conf->_state = CONFERENCE_STATE_HOLD;

    conf = new_conf;
}

void free_conference_obj_t (conference_obj_t *c)
{
    g_free (c->_confID);
    g_free (c);
}
