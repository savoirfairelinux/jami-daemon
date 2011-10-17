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

#include "calltab.h"
#include "callable_obj.h"
#include "calltree.h"
#include "conferencelist.h"
#include "logger.h"

static gint is_confID_confstruct(gconstpointer a, gconstpointer b)
{
    conference_obj_t * c = (conference_obj_t*) a;
    return g_strcasecmp(c->_confID, (const gchar*) b);
}

void conferencelist_init(calltab_t *tab)
{
    if (tab == NULL) {
        ERROR("ConferenceList: Error: Call tab is NULL");
        return;
    }

    tab->conferenceQueue = g_queue_new();
}


void conferencelist_clean(calltab_t *tab)
{
    if (tab == NULL) {
        ERROR("ConferenceList: Error: Calltab tab is NULL");
        return;
    }

    g_queue_free(tab->conferenceQueue);
}

void conferencelist_clean_history(void)
{
    while (conferencelist_get_size(history_tab) > 0) {
        conference_obj_t *conf = conferencelist_pop_head(history_tab);

        if (conf)
            calltree_remove_conference(history_tab, conf);
        else
            ERROR("ConferenceList: Conference pointer is NULL");
    }
}


void conferencelist_reset(calltab_t *tab)
{
    if (tab == NULL) {
        ERROR("ConferenceList: Error: Calltab tab is NULL");
        return;
    }

    g_queue_free(tab->conferenceQueue);
    tab->conferenceQueue = g_queue_new();
}


void conferencelist_add(calltab_t *tab, const conference_obj_t* conf)
{
    if (conf == NULL) {
        ERROR("ConferenceList: Error: Conference is NULL");
        return;
    }

    if (tab == NULL) {
        ERROR("ConferenceList: Error: Tab is NULL");
        return;
    }

    conference_obj_t *c = conferencelist_get(tab, conf->_confID);

    // only add conference into the list if not already inserted
    if (c == NULL)
        g_queue_push_tail(tab->conferenceQueue, (gpointer) conf);
}


void conferencelist_remove(calltab_t *tab, const gchar* const conf)
{
    DEBUG("ConferenceList: Remove conference %s", conf);

    if (conf == NULL) {
        ERROR("ConferenceList: Error: Conf id is NULL");
        return;
    }

    if (tab == NULL) {
        ERROR("ConferenceList: Error: Calltab is NULL");
        return;
    }

    gchar *c = (gchar*) conferencelist_get(tab, conf);

    if (c == NULL) {
        ERROR("ConferenceList: Error: Could not find conference %s", conf);
        return;
    }

    g_queue_remove(tab->conferenceQueue, c);
}

conference_obj_t* conferencelist_get(calltab_t *tab, const gchar* const conf_id)
{
    DEBUG("ConferenceList: Conference list get %s", conf_id);

    if (tab == NULL) {
        ERROR("ConferenceList: Error: Calltab is NULL");
        return NULL;
    }

    GList *c = g_queue_find_custom(tab->conferenceQueue, conf_id, is_confID_confstruct);

    if (c == NULL) {
        ERROR("ConferenceList: Error: Could not find conference %s", conf_id);
        return NULL;
    }

    return (conference_obj_t*) c->data;
}

conference_obj_t* conferencelist_get_nth(calltab_t *tab, guint n)
{
    if (tab == NULL) {
        ERROR("ConferenceList: Error: Calltab is NULL");
        return NULL;
    }

    conference_obj_t *c = g_queue_peek_nth(tab->conferenceQueue, n);

    if (c == NULL) {
        ERROR("ConferenceList: Error: Could not fetch conference %d", n);
        return NULL;
    }

    return c;
}

conference_obj_t *conferencelist_pop_head(calltab_t *tab)
{
    if (tab == NULL) {
        ERROR("ConferenceList: Error: Tab is NULL");
        return NULL;
    }

    return g_queue_pop_head(tab->conferenceQueue);
}

guint conferencelist_get_size(calltab_t *tab)
{
    if (tab == NULL) {
        ERROR("ConferenceList: Error: Calltab is NULL");
        return 0;
    }

    return g_queue_get_length(tab->conferenceQueue);
}
