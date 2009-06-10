/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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

#ifndef __CALLTREE_H__
#define __CALLTREE_H__

#include <gtk/gtk.h>
#include <calltab.h>
#include <mainwindow.h>

/** @file calltree.h
  * @brief The GtkTreeView that list calls in the main window.
  */

/**
 * Create a new widget calltree
 * @return GtkWidget* A new widget
 */
void
calltree_create(calltab_t* tab, gboolean searchbar_type);

/**
 * Add a call in the calltree
 * @param c The call to add
 */
void
calltree_add_call (calltab_t* ct, callable_obj_t * c);

/*
 * Update the call tree if the call state changes
 * @param c The call to update
 */
void
calltree_update_call (calltab_t* ct, callable_obj_t * c);

/**
 * Remove a call from the call tree
 * @param c The call to remove
 */
void
calltree_remove_call (calltab_t* ct, callable_obj_t * c);

void
calltree_reset (calltab_t* tab);

void
calltree_display (calltab_t *tab);

void
row_activated(GtkTreeView *, GtkTreePath *, GtkTreeViewColumn *, void *);

#endif
