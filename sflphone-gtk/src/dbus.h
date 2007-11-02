/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc@squidy.info>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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


#ifndef __DBUS_H__
#define __DBUS_H__

#include <accountlist.h>
#include <calllist.h>

/** @file dbus.h
  * @brief General DBus functions wrappers.
  */
/** @return TRUE if connection succeeded, FALSE otherwise */
gboolean dbus_connect ();
void dbus_clean ();

/* CallManager */
void dbus_hold (const call_t * c );
void dbus_unhold (const call_t * c );
void dbus_hang_up (const call_t * c );
void dbus_transfert (const call_t * c);
void dbus_accept (const call_t * c);
void dbus_refuse (const call_t * c);
void dbus_place_call (const call_t * c);

/* ConfigurationManager */
/** Returns a NULL terminated array of gchar pointers */
gchar ** dbus_account_list();
GHashTable * dbus_account_details(gchar * accountID);
void dbus_set_account_details(account_t *a);
void dbus_add_account(account_t *a);
void dbus_remove_account(gchar * accountID);
void dbus_set_volume(const gchar * device, gdouble value);
gdouble dbus_get_volume(const gchar * device);
void dbus_play_dtmf(const gchar * key);

/* Instance */
void dbus_register( int pid, gchar * name);
void dbus_unregister(int pid);



#endif
