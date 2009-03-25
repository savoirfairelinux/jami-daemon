/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
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
 */

#include <calltab.h>
#include <gtk/gtk.h>
#include <stdlib.h>
#include <calltree.h>
#include <contacts/searchfilter.h>

calltab_t*
calltab_init(gchar* searchbar_type)
{
	calltab_t* ret;

	ret = malloc(sizeof(calltab_t));

	ret->store = NULL;
	ret->view = NULL;
	ret->tree = NULL;
        ret->searchbar = NULL;
	ret->callQueue = NULL;
	ret->selectedCall = NULL;
        // ret->histfilter = NULL;

	create_call_tree(ret, searchbar_type);
	call_list_init(ret);


	return ret;
}

void
call_select (calltab_t* tab, call_t * c )
{
  tab->selectedCall = c;
}


call_t *
call_get_selected (calltab_t* tab)
{
  return tab->selectedCall;
}

void
create_searchbar(calltab_t* tab, gchar* searchbar_type)
{
  // g_strcmp0 returns 0 if str1 == str2
  if(g_strcmp0(searchbar_type,"history") == 0){

      tab->searchbar = create_filter_entry_history();

  }

  else if(g_strcmp0(searchbar_type,"contacts") == 0)
      tab->searchbar = create_filter_entry_contact();
}
