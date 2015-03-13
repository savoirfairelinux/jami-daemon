/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
#include <gtk/gtk.h>
#include <stdlib.h>
#include "str_utils.h"
#include "calltree.h"
#include "contacts/searchbar.h"

calltab_t*
calltab_init(gboolean has_searchbar, const gchar * const name, SFLPhoneClient *client)
{
    calltab_t* ret = g_new0(calltab_t, 1);
    ret->name = g_strdup(name);

    calltree_create(ret, has_searchbar, client);

    ret->callQueue = g_queue_new();
    ret->selectedCall = NULL;
    ret->mainwidget =  NULL;

    return ret;
}

void
calltab_select_call(calltab_t* tab, callable_obj_t * c)
{
    g_assert(tab);
    g_debug("Select call %s", c ? c->_callID : "");

    tab->selectedType = A_CALL;
    tab->selectedCall = c;
    tab->selectedConf = NULL;
}


void
calltab_select_conf(calltab_t *tab, conference_obj_t * c)
{
    g_assert(tab);
    g_debug("Selected conf %s", c ? c->_confID : "");

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
    g_return_val_if_fail(!calllist_empty(tab), NULL);
    return tab->selectedCall;
}

conference_obj_t*
calltab_get_selected_conf(calltab_t *tab)
{
    g_return_val_if_fail(!calllist_empty(tab), NULL);
    return tab->selectedConf;
}

void
calltab_create_searchbar(calltab_t* tab, SFLPhoneClient *client)
{
    g_assert(tab);

    if (calltab_has_name(tab, HISTORY))
        tab->searchbar = history_searchbar_new(client->settings);
    else if (calltab_has_name(tab, CONTACTS))
        tab->searchbar = contacts_searchbar_new(client->settings);
    else
        g_warning("Current calls tab does not need a searchbar\n");
}

gboolean
calltab_has_name(calltab_t *tab, const gchar *name)
{
    return g_strcmp0(tab->name, name) == 0;
}
