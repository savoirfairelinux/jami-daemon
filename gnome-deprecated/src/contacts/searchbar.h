/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *
 *  Author: Antoine Reversat <antoine.reversat@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
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

/**
 * This file contains functions relative to search bar used with history and
 * addressbook.
 */

#ifndef __SEARCH_FILTER_H__
#define __SEARCH_FILTER_H__

#include <gtk/gtk.h>
#include "history.h"

/**
 * Create a new search bar for call hostory
 */
GtkWidget*
history_searchbar_new(GSettings *settings);

/**
 * Create a new search bar for addressbook
 */
GtkWidget*
contacts_searchbar_new(GSettings *settings);

/**
 * Get type of call to be search from call history
 */
SearchType
get_current_history_search_type();

/**
 * Set focus on addressbook search bar
 */
void
set_focus_on_addressbook_searchbar();

/**
 * Reload combo box to update list of active addressbook
 */
void
update_searchbar_addressbook_list(GSettings *settings);

/**
 * Create a new menu listing all system addressbooks
 */
GtkWidget *
addressbook_menu_new(GSettings *settings);

#endif
