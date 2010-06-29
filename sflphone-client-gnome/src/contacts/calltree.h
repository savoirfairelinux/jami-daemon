/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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

#ifndef __CALLTREE_H__
#define __CALLTREE_H__

#include <gtk/gtk.h>
#include <calltab.h>
#include <mainwindow.h>

#define SFLPHONE_HIG_MARGIN 10
#define CALLTREE_CALL_ICON_WIDTH 24
#define CALLTREE_SECURITY_ICON_WIDTH 24
#define CALLTREE_TEXT_WIDTH (MAIN_WINDOW_WIDTH - CALLTREE_SECURITY_ICON_WIDTH - CALLTREE_CALL_ICON_WIDTH - (2*SFLPHONE_HIG_MARGIN))

/** @file calltree.h
  * @brief The GtkTreeView that list calls in the main window.
  */

enum {
    A_CALL,
    A_CONFERENCE
};


/**
 * Tags used to identify display type in calltree
 */
typedef enum {
    DISPLAY_TYPE_CALL,
    DISPLAY_TYPE_CALL_TRANSFER,
    DISPLAY_TYPE_SAS,
    DISPLAY_TYPE_STATE_CODE,
    DISPLAY_TYPE_HISTORY
} CallDisplayType;

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
calltree_add_call (calltab_t* ct, callable_obj_t * c, GtkTreeIter *parent);

/*
 * Update the call tree if the call state changes
 * @param c The call to update
 */
void
calltree_update_call (calltab_t* ct, callable_obj_t * c, GtkTreeIter *parent);

/**
 * Remove a call from the call tree
 * @param c The call to remove
 */
void
calltree_remove_call (calltab_t* ct, callable_obj_t * c, GtkTreeIter *parent);

void 
calltree_add_history_entry (callable_obj_t * c);

void
calltree_add_conference (calltab_t* tab, conference_obj_t* conf);

void
calltree_update_conference (calltab_t* tab, const gchar* confID);

void
calltree_remove_conference (calltab_t* tab, const conference_obj_t* conf, GtkTreeIter *parent);

void
calltree_reset (calltab_t* tab);

void
calltree_display (calltab_t *tab);

void
row_activated(GtkTreeView *, GtkTreePath *, GtkTreeViewColumn *, void *);

#endif
