/*
 *  Copyright (C) 2008 2009 Savoir-Faire Linux inc.
 *
 *  Author: Antoine Reversat <antoine.reversat@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#ifndef __SEARCH_FILTER_H__
#define __SEARCH_FILTER_H__

#include <calllist.h>
#include <gtk/gtk.h>
#include <libsexy/sexy-icon-entry.h>


GdkPixbuf *waitingPixOff;

GtkTreeModel* create_filter(GtkTreeModel* child);

gboolean is_visible(GtkTreeModel* model, GtkTreeIter* iter, gpointer data);

GtkWidget* create_filter_entry_contact();

GtkWidget* create_filter_entry_history();

void activateWaitingLayer();

void deactivateWaitingLayer();

#endif
