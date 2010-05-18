/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#ifndef __CONFERENCELIST_H__
#define __CONFERENCELIST_H__


#include <conference_obj.h>
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
conferencelist_add (const conference_obj_t* conf);

/** This function remove a conference from list.
  * @param callID The callID of the conference you want to remove
  */
void
conferencelist_remove (const gchar* conf);

/** Return the number of calls in the list
  * @return The number of calls in the list */
guint
conferencelist_get_size ();

/** Return the call at the nth position in the list
  * @param n The position of the call you want
  * @return A call or NULL */
conference_obj_t*
conferencelist_get_nth (guint n );

/** Return the call corresponding to the callID
  * @param n The callID of the call you want
  * @return A call or NULL */
conference_obj_t*
conferencelist_get (const gchar* conf);


#endif
