/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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
 * This file contains functions specific for history.
 */

#ifndef __HISTORY_H__
#define __HISTORY_H__

#include <gtk/gtk.h>
#include <sflphone_const.h>

/**
 * Execute a search in history
 */
void
history_search(GtkEntry* entry UNUSED);

/**
 * Initialize history
 */
void
history_init();

/**
 * Set history search bar widget (needed for is_visible)
 */
void
history_set_searchbar_widget(GtkWidget *);

#endif
