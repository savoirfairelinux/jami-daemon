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

#include <timestamp.h>

  gchar* 
timestamp_get_call_date( void )
{
  struct tm* ptr;
  time_t lt;

  lt = time(NULL);
  ptr = localtime(&lt);

  return (gchar*) asctime( ptr ) ;
}

  struct tm*
what_time_is_it( void )
{
  time_t lt = time(NULL);
  return localtime(&lt);
}

  gchar*
process_call_duration( call_t* c )
{
  gchar * res;
  g_print("Start = %i - Stop = %i  - Call duration = %i\n", c->_start , c->_stop , (int)(c->_stop - c->_start));
  int duration = c->_stop - c->_start;

  if( duration / 60 == 0 )
  {
    if( duration < 10 )
      res = g_markup_printf_escaped("\n00:0%i", duration);
    else
      res = g_markup_printf_escaped("\n00:%i", duration);
  }
  else
  {
    if( duration%60 < 10 )
      res = g_markup_printf_escaped("\n%i:0%i" , duration/60 , duration%60);
    else
      res = g_markup_printf_escaped("\n%i:%i" , duration/60 , duration%60);
  }
  return res;
}




















