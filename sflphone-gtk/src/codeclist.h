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
  /** Payload of the codec */
  int _payload;
  /** Tells if the codec has been activated */
  gboolean is_active;
  /** String description */
  gchar * name;
  /** Sample rate */
  int sample_rate;
  /** Bitrate */
  gdouble _bitrate;
  /** Bandwidth */
  gdouble _bandwidth;
}codec_t;

/** @struct codec_t
  * @brief Codec information.
  * This struct holds information about a codec.
  * This match how the server internally works and the dbus API to save and retrieve the codecs details.
  */

/** 
 * This function initialize the codec list. 
 */
void codec_list_init();

/** 
 * This function empty and free the codec list. 
 */
void codec_list_clear();

/** 
 * This function append an codec to list. 
 * @param c The codec you want to add 
 */
void codec_list_add(codec_t * c);

/**
 * Set a codec active. An active codec will be used for codec negociation
 * @param name The string description of the codec
 */
void codec_set_active(gchar* name);

/**
 * Set a codec inactive. An active codec won't be used for codec negociation
 * @param name The string description of the codec
 */
void codec_set_inactive(gchar* name);

/** 
 * Return the number of codecs in the list
 * @return guint The number of codecs in the list 
 */
guint codec_list_get_size();

/** 
 * Return the codec structure that corresponds to the string description 
 * @param name The string description of the codec
 * @return codec_t* A codec or NULL 
 */
codec_t * codec_list_get(const gchar * name);

/** 
 * Return the codec at the nth position in the list
 * @param index The position of the codec you want
 * @return codec_t* A codec or NULL 
 */
codec_t* codec_list_get_nth(guint index);

/**
 * Set the prefered codec first in the codec list
 * @param index The position in the list of the prefered codec
 */ 
void codec_set_prefered_order(guint index);

/** 
 * Move the codec from an unit up in the codec_list
 * @param index The current index in the list
 */
void codec_list_move_codec_up(guint index);

/** 
 * Move the codec from an unit down in the codec_list
 * @param index The current index in the list
 */
void codec_list_move_codec_down(guint index);

/**
 * Notify modifications on codecs to the server
 */
void codec_list_update_to_daemon();

#endif
