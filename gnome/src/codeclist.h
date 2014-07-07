/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.net>
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

#ifndef __CODECLIST_H__
#define __CODECLIST_H__

#include <gtk/gtk.h>
#include "accountlist.h"
/** @file codeclist.h
  * @brief A list to hold codecs.
  */

typedef struct {
    /** Payload of the codec */
    gint payload;
    /** Tells if the codec has been activated */
    gboolean is_active;
    /** String description */
    gchar * name;
    /** Sample rate */
    gint sample_rate;
    /** Bitrate */
    gchar * bitrate;
    /** Channel number */
    gint channels;
} codec_t;

/** @struct codec_t
  * @brief Codec information.
  * This struct holds information about a codec.
  * This match how the server internally works and the dbus API to save and retrieve the codecs details.
  */

/**
 * This function initialize the system wide codec list.
 * @return FALSE if initialization failed
 */
gboolean codecs_load (void);

/**
 * This function empty and free the system wide codec list.
 */
void codecs_unload (void);

/**
 * Return the codec structure that corresponds to the string description
 * @param name The string description of the codec
 * @return codec_t* A codec or NULL
 */
codec_t * codec_list_get_by_name(gconstpointer name, GQueue *q);

/**
 * Set the preferred codec first in the codec list
 * @param index The position in the list of the preferred codec
 */
void codec_set_preferred_order(guint index, GQueue *q);

/**
 * Notify modifications on codecs to the server
 */
void codec_list_update_to_daemon(const account_t *acc);

codec_t* codec_list_get_by_payload(gint payload, GQueue *q);

GQueue* get_audio_codecs_list(void);

/*
 * Attach a codec list to a specific account
 *
 * @param acc		A pointer on the account to modify
 */
void account_create_codec_list(account_t **acc);

codec_t *codec_create_new_from_caps(codec_t *original);

void codec_set_active(codec_t *c, gboolean active);

void codec_list_move_codec_up(guint codec_index, GQueue **q);
void codec_list_move_codec_down(guint codec_index, GQueue **q);

#endif
