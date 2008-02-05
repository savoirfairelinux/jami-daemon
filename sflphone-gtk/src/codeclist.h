/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.net>
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

#ifndef __CODECLIST_H__
#define __CODECLIST_H__

#include <gtk/gtk.h>
/** @file codeclist.h
  * @brief A list to hold codecs.
  */

typedef struct {
  int _payload;
  gboolean is_active;
  gchar * name;
  int sample_rate;
  gdouble _bitrate;
  gdouble _bandwidth;
}codec_t;

void codec_list_init();
void codec_list_clear();
void codec_list_add(codec_t * c);
void codec_set_active(gchar* name);
void codec_set_inactive(gchar* name);
guint codec_list_get_size();
codec_t * codec_list_get(const gchar * name);
//codec_t * codec_list_get(const int payload);
codec_t* codec_list_get_nth(guint index);

/**
 * Set the prefered codec first in the codec list
 * @param index The position in the list of the prefered codec
 */ 
void codec_set_prefered_order(guint index);
//gchar * codec_get_name(codec_t * c);
//guint codec_get_rate(gchar * codec_name);

void codec_list_move_codec_up(guint index);
void codec_list_move_codec_down(guint index);

#endif
