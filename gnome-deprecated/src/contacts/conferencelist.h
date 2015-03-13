/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#ifndef __CONFERENCELIST_H__
#define __CONFERENCELIST_H__

/** @file conferencelist.h
  * @brief A list to store conferences.
  */

/** This function initialize a conference list. */
void conferencelist_init (calltab_t *);

/** This function empty and free the conference list. */
void conferencelist_clean (calltab_t *);

/** This function empty, free the conference list and allocate a new one. */
void conferencelist_reset (calltab_t *);

/** This function append a conference to the list.
  * @param conf The conference you want to add
  * */
void conferencelist_add (calltab_t *, const conference_obj_t *);

/** This function remove a conference from list.
  * @param callID The callID of the conference you want to remove
  */
void conferencelist_remove (calltab_t *, const gchar * const conf_id);

/** Return the number of calls in the list
  * @return The number of calls in the list */
guint conferencelist_get_size (calltab_t *);

/** Return the call at the nth position in the list
  * @param n The position of the call you want
  * @return A call or NULL */
conference_obj_t* conferencelist_get_nth (calltab_t *, guint);

/** Return the call corresponding to the callID
  * @param n The callID of the call  want
  * @return A call or NULL */
conference_obj_t* conferencelist_get(calltab_t *, const gchar const *);

conference_obj_t* conferencelist_pop_head(calltab_t *);

#endif
