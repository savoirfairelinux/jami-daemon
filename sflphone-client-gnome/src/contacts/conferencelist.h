/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#ifndef __CONFERENCELIST_H__
#define __CONFERENCELIST_H__

#include <gtk/gtk.h>

/** @file conferencelist.h
  * @brief A list to store conferences.
  */

GQueue* conferenceQueue;

/** This function initialize a conference list. */
void
conferencelist_init ();

/** This function empty and free the conference list. */
void
conferencelist_clean ();

/** This function empty, free the conference list and allocate a new one. */
void
conferencelist_reset ();

/** This function append a conference to the list.
  * @param conf The conference you want to add
  * */
void
conferencelist_add (const gchar* conf_id);

/** This function remove a conference from list.
  * @param callID The callID of the conference you want to remove
  */
void
conferencelist_remove (const gchar* conf_id);

/** Return the number of calls in the list
  * @return The number of calls in the list */
guint
conferencelist_get_size (const gchar* conf_id);

/** Return the call at the nth position in the list
  * @param n The position of the call you want
  * @return A call or NULL */
gchar*
conferencelist_get_nth (const gchar* conf_id, guint n );

/** Return the call corresponding to the callID
  * @param n The callID of the call you want
  * @return A call or NULL */
gchar*
conferencelist_get (const gchar* conf_id);


#endif
