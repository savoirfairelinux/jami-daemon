/*
 *  Copyright (C) 2008 2009 Savoir-Faire Linux inc.
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

/**
 * This file contains functions relative to search bar used with history and
 * addressbook.
 */

#ifndef __SEARCH_FILTER_H__
#define __SEARCH_FILTER_H__

#include <calllist.h>
#include <gtk/gtk.h>
#include <mainwindow.h>

// From version 2.16, gtk provides the functionalities libsexy used to provide
#if GTK_CHECK_VERSION(2,16,0)
#else
#include <libsexy/sexy-icon-entry.h>
#endif

#include <addressbook.h>
#include <history.h>

GdkPixbuf *waitingPixOff;

SearchType HistorySearchType;

/**
 * Create a new search bar with "type" passed in
 * parameter
 */
GtkWidget* history_searchbar_new (void);
GtkWidget* contacts_searchbar_new (void);

SearchType get_current_history_search_type (void);


/**
 * Initialize a specific search bar
 */
void searchbar_init(calltab_t *);

/**
 * Activate a waiting layer during search
 */
void activateWaitingLayer();

/**
 * Deactivate waiting layer
 */
void deactivateWaitingLayer();

#endif
