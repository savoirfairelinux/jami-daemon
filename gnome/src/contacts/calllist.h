/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#ifndef __CALLLIST_H__
#define __CALLLIST_H__

#include <gtk/gtk.h>
#include "callable_obj.h"
#include "conference_obj.h"
#include "sflphone_client.h"

static const gchar * const CURRENT_CALLS = "current_calls";
static const gchar * const HISTORY = "history";
static const gchar * const CONTACTS = "contacts";

/** @file calllist.h
  * @brief A list to hold calls.
  */

typedef struct {
    GtkTreeStore* store;
    GtkWidget* view;
    GtkWidget* tree;
    GtkWidget* searchbar;
    GtkWidget* mainwidget;

    // Calllist vars
    GQueue* callQueue;
    GQueue* conferenceQueue;
    gint selectedType;
    callable_obj_t* selectedCall;
    conference_obj_t* selectedConf;
    gchar *name;
} calltab_t;

void
calllist_add_contact(gchar *, gchar *, contact_type_t, GdkPixbuf *);

/** This function empty and free the call list. */
void
calllist_clean(calltab_t* tab);

/** This function empty, free the call list and allocate a new one. */
void
calllist_reset(calltab_t* tab);

/** Get the maximun number of calls in the history calltab */
gdouble
call_history_get_max_calls(void);

/** Set the maximun number of calls in the history calltab */
void
call_history_set_max_calls(const gdouble number);

/** This function append a call to list.
  * @param c The call you want to add
  * */
void
calllist_add_call(calltab_t* tab, callable_obj_t * c);

/** This function inserts a call to front of list.
  * @param c The call you want to add
  * */
void
calllist_add_call_to_front(calltab_t* tab, callable_obj_t * c);

/** This function remove a call from list.
  * @param callID The callID of the call you want to remove
  */
void
calllist_remove_call(calltab_t* tab, const gchar * callID, SFLPhoneClient *client);

/** Return the first call that corresponds to the state.
  * This is usefull for unique states as DIALING and CURRENT.
  * @param state The state
  * @return A call or NULL */
callable_obj_t *
calllist_get_by_state(calltab_t* tab, call_state_t state);

gboolean
calllist_empty(calltab_t *tab);

/** Return the number of calls in the list
  * @return The number of calls in the list */
guint
calllist_get_size(const calltab_t* tab);

/** Return the call at the nth position in the list
  * @param n The position of the call you want
  * @return A call or NULL */
callable_obj_t*
calllist_get_nth(calltab_t* tab, guint n);

/** Return the call corresponding to the callID
  * @param n The callID of the call you want
  * @return A call or NULL */
callable_obj_t *
calllist_get_call(calltab_t* tab, const gchar * callID);

/**
 * Clean the history. Delete all calls
 */
void
calllist_clean_history();

/**
 * Remove one specified call from the history list
 * @param c The call to remove
 */
void
calllist_remove_from_history(callable_obj_t* c, SFLPhoneClient *client);

/**
 * Initialize a non-empty call list
 */
void
calllist_set_list(calltab_t* tab, gchar **call_list);

#endif
