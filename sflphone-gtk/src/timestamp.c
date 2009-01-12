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
    unsigned char str[100];

    time(&lt);
    ptr = gmtime(&lt);

    // result function of the current locale
    strftime((char *)str, 100, "%c",
                               (const struct tm *)ptr);
    return g_markup_printf_escaped("\n%s\n" , str);
}

  gchar*
process_call_duration( call_t* c )
{
  gchar * res;
  g_print("Start = %i - Stop = %i  - Call duration = %i\n", (int)c->_start , (int)c->_stop , (int)(c->_stop - c->_start));

  if( c->history_state == MISSED && c->_stop == 0 )
    return g_markup_printf_escaped(_("<small>Missed call</small>"));

  int duration = c->_stop - c->_start;

  if( duration / 60 == 0 )
  {
    if( duration < 10 )
      res = g_markup_printf_escaped("00:0%i", duration);
    else
      res = g_markup_printf_escaped("00:%i", duration);
  }
  else
  {
    if( duration%60 < 10 )
      res = g_markup_printf_escaped("%i:0%i" , duration/60 , duration%60);
    else
      res = g_markup_printf_escaped("%i:%i" , duration/60 , duration%60);
  }
  return g_markup_printf_escaped(_("<small>Duration:</small> %s"), res);
}

