/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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
#include "str_utils.h"
#include "callable_obj.h"
#include "calltree.h"
#include "conferencelist.h"

static gint is_confID_confstruct(gconstpointer a, gconstpointer b)
{
    conference_obj_t * c = (conference_obj_t*) a;
    return utf8_case_cmp(c->_confID, (const gchar*) b);
}

void conferencelist_init(calltab_t *tab)
{
    if (tab == NULL) {
        g_warning("Call tab is NULL");
        return;
    }

    tab->conferenceQueue = g_queue_new();
}


void conferencelist_clean(calltab_t *tab)
{
    if (tab == NULL) {
        g_warning("Calltab tab is NULL");
        return;
    }

    g_queue_free(tab->conferenceQueue);
}

void conferencelist_reset(calltab_t *tab)
{
    if (tab == NULL) {
        g_warning("Calltab tab is NULL");
        return;
    }

    g_queue_free(tab->conferenceQueue);
    tab->conferenceQueue = g_queue_new();
}


void conferencelist_add(calltab_t *tab, const conference_obj_t* conf)
{
    if (conf == NULL) {
        g_warning("Conference is NULL");
        return;
    }

    if (tab == NULL) {
        g_warning("Tab is NULL");
        return;
    }

    conference_obj_t *c = conferencelist_get(tab, conf->_confID);

    // only add conference into the list if not already inserted
    if (c == NULL)
        g_queue_push_tail(tab->conferenceQueue, (gpointer) conf);
}


void conferencelist_remove(calltab_t *tab, const gchar* const conf_id)
{
    g_debug("Remove conference %s", conf_id);

    if (conf_id == NULL) {
        g_warning("Conf id is NULL");
        return;
    }

    if (tab == NULL) {
        g_warning("Calltab is NULL");
        return;
    }

    conference_obj_t *c =  conferencelist_get(tab, conf_id);

    if (c == NULL)
        return;

    g_queue_remove(tab->conferenceQueue, c);
}

conference_obj_t* conferencelist_get(calltab_t *tab, const gchar* const conf_id)
{
    g_debug("Conference list get %s", conf_id);

    if (tab == NULL) {
        g_warning("Calltab is NULL");
        return NULL;
    }

    GList *c = g_queue_find_custom(tab->conferenceQueue, conf_id, is_confID_confstruct);

    if (c == NULL)
        return NULL;

    return (conference_obj_t*) c->data;
}

conference_obj_t* conferencelist_get_nth(calltab_t *tab, guint n)
{
    if (tab == NULL) {
        g_warning("Calltab is NULL");
        return NULL;
    }

    conference_obj_t *c = g_queue_peek_nth(tab->conferenceQueue, n);

    if (c == NULL) {
        g_warning("Could not fetch conference %d", n);
        return NULL;
    }

    return c;
}

conference_obj_t *conferencelist_pop_head(calltab_t *tab)
{
    if (tab == NULL) {
        g_warning("Tab is NULL");
        return NULL;
    }

    return g_queue_pop_head(tab->conferenceQueue);
}

guint conferencelist_get_size(calltab_t *tab)
{
    if (tab == NULL) {
        g_warning("Calltab is NULL");
        return 0;
    }

    return g_queue_get_length(tab->conferenceQueue);
}
