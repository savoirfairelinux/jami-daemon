/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

/** @file calltree.h
  * @brief The GtkTreeView that list calls in the main window.
  */

typedef enum {
    A_CALL,
    A_CONFERENCE,
    A_INVALID
} CallType;

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

struct calltab_t;
struct callable_obj_t;
struct conference_obj_t;

/**
 * Create a new widget calltree
 * @return GtkWidget* A new widget
 */
void
calltree_create (calltab_t *, int searchbar_type);

/**
 * Add a call in the calltree
 * @param c The call to add
 */
void
calltree_add_call (calltab_t *, callable_obj_t *, GtkTreeIter *);

void
calltree_add_call_to_conference_in_history(callable_obj_t *call, const gchar * const confID);

/*
 * Update the call tree if the call state changes
 * @param c The call to update
 */
void
calltree_update_call (calltab_t *, callable_obj_t *);

/**
 * Remove a call from the call tree
 * @param c The call to remove
 */
void
calltree_remove_call(calltab_t *, callable_obj_t *);

/**
 * Add a callable object to history treeview
 * @param The callable object to be inserted into the history
 * @param The parent item in case of a conference, should be NULL in case of a normal call
 */
void
calltree_add_history_entry (callable_obj_t *, GtkTreeIter *);

void
calltree_add_conference_to_current_calls(conference_obj_t *);

void
calltree_add_conference_to_history(conference_obj_t *);

void
calltree_remove_conference(calltab_t *, const conference_obj_t *);

void
calltree_display (calltab_t *);

void
row_activated (GtkTreeView *, GtkTreePath *, GtkTreeViewColumn *, void *);

/**
 * Update elapsed time based on selected calltree's call
 */
gboolean
calltree_update_clock(gpointer);

/**
 * @param The calltab (current_calls, history, contacts)
 * @param The call
 * @param The callID/confID
 */
void
calltree_(calltab_t *, callable_obj_t *, const gchar * const);

#endif
