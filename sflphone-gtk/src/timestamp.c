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

gchar* months[12] = {"january","february","march", "april", "may", "june", "july", "august", "september", "october", "november", "december"};
gchar* week_days[7] = {"Sun", "Mon", "Tue","Wed","Thu","Fri","Sat"};

  gchar* 
timestamp_get_call_date( void )
{
  struct tm* ptr;
  time_t lt;

  lt = time(NULL);
  ptr = localtime(&lt);


  return format( ptr ) ;
}

  gchar*
process_call_duration( call_t* c )
{
  gchar * res;
  g_print("Start = %i - Stop = %i  - Call duration = %i\n", c->_start , c->_stop , (int)(c->_stop - c->_start));

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

gchar*
format( struct tm* ptr )
{
  gchar *result;

  gchar *hour, *min;
  gchar *day_of_week, *month, *day_number;

  if( ptr->tm_hour < 10 )
    hour = g_markup_printf_escaped("0%i", ptr->tm_hour);
  else
    hour = g_markup_printf_escaped("%i", ptr->tm_hour);
  
  if( ptr->tm_min < 10 )
    min = g_markup_printf_escaped("0%i", ptr->tm_min);
  else
    min = g_markup_printf_escaped("%i", ptr->tm_min);

  day_of_week = g_markup_printf_escaped( "%i", ptr->tm_mday );

  month = months[ptr->tm_mon];
  day_number = week_days[ptr->tm_wday];

  result = g_markup_printf_escaped( "\n%s %s %s %s:%s\n" , day_number, month , day_of_week , hour, min );

  return result;

}


















