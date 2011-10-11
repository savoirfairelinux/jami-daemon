/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Antoine Reversat <antoine.reversat@savoirfairelinux.com>
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
#include <gtk/gtk.h>
#include <stdlib.h>
#include "calltree.h"
#include "contacts/searchbar.h"
#include "logger.h"

calltab_t* calltab_init(gboolean searchbar_type, const gchar * const name)
{
    calltab_t* ret = calloc(1, sizeof(calltab_t));
    ret->_name = g_strdup(name);

    calltree_create(ret, searchbar_type);

    ret->callQueue = g_queue_new();
    ret->selectedCall = NULL;

    return ret;
}

void
calltab_select_call(calltab_t* tab, callable_obj_t * c)
{
    g_assert(tab);
    DEBUG("CallTab: Select call %s", c ? c->_callID : "");

    tab->selectedType = A_CALL;
    tab->selectedCall = c;
    tab->selectedConf = NULL;
}


void
calltab_select_conf(calltab_t *tab, conference_obj_t * c)
{
    g_assert(tab);
    DEBUG("CallTab: Selected conf %s", c ? c->_confID : "");

    tab->selectedType = A_CONFERENCE;
    tab->selectedConf = c;
    tab->selectedCall = NULL;
}

gint
calltab_get_selected_type(calltab_t* tab)
{
    g_assert(tab);
    return tab->selectedType;
}

callable_obj_t *
calltab_get_selected_call(calltab_t *tab)
{
    g_assert(tab);
    return tab->selectedCall;
}

conference_obj_t*
calltab_get_selected_conf(calltab_t *tab)
{
    g_assert(tab);
    return tab->selectedConf;
}

void
calltab_create_searchbar(calltab_t* tab)
{
    g_assert(tab);

    if (g_strcasecmp(tab->_name, HISTORY) == 0)
        tab->searchbar = history_searchbar_new();
    else if (g_strcasecmp(tab->_name, CONTACTS) == 0)
        tab->searchbar = contacts_searchbar_new();
    else
        ERROR("Current calls tab does not need a searchbar\n");
}
