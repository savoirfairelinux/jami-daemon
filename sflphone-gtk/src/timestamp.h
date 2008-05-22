/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
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


#ifndef __TIMESTAMP_H__
#define __TIMESTAMP_H__

/** @file timestamp.h
  * @brief Introduces time notion in SFLphone, like call duration, call time, etc...
  * Useful for the history
  */

#include <time.h>
#include <gtk/gtk.h>

#include <calllist.h>
#include <sflphone_const.h>

gchar* timestamp_get_call_date( void );

gchar* process_call_duration( call_t* c );

gchar* format( struct tm* ptr );

#endif
