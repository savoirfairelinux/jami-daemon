/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include "codeclist.h"
#include "logger.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "dbus.h"

static GQueue * codecsCapabilities = NULL;

gint
is_name_codecstruct(gconstpointer a, gconstpointer b)
{
    const codec_t *c = a;
    return !!g_strcmp0(c->name, (const gchar *) b);
}

static gint
is_payload_codecstruct(gconstpointer a, gconstpointer b)
{
    const codec_t *c = a;
    return (c->_payload == GPOINTER_TO_INT(b)) ? 0 : 1;
}

static codec_t *codec_create_new(gint payload)
{
    gchar **specs = dbus_audio_codec_details(payload);

    codec_t *codec = g_new0(codec_t, 1);
    codec->_payload = payload;
    codec->name = strdup(specs[0]);
    codec->sample_rate = atoi(specs[1]);
    codec->_bitrate = atoi(specs[2]);
    codec->is_active = TRUE;

    g_strfreev(specs);

    return codec;
}


void codec_capabilities_load(void)
{
    // Create the queue object that will contain the global list of audio codecs
    if (codecsCapabilities != NULL)
        g_queue_free(codecsCapabilities);

    codecsCapabilities = g_queue_new();

    // This is a global list inherited by all accounts
    GArray *codecs = dbus_audio_codec_list();

    // Add the codecs in the list
    for (guint i = 0; i < codecs->len; i++) {
        gint payload = g_array_index(codecs, gint, i);
        codec_t *c = codec_create_new(payload);
        g_queue_push_tail(codecsCapabilities, (gpointer) c);
    }

    g_array_unref(codecs);

    // If we didn't load any codecs, problem ...
    if (g_queue_get_length(codecsCapabilities) == 0)
        ERROR("No audio codecs found");
}

codec_t *codec_create_new_from_caps(codec_t *original)
{
    codec_t *codec = NULL;

    if (original) {
        codec = g_new0(codec_t, 1);
        codec->_payload = original->_payload;
        codec->name = original->name;
        codec->sample_rate = original->sample_rate;
        codec->_bitrate = original->_bitrate;
        codec->is_active = TRUE;
    }

    return codec;
}


void codec_list_clear(GQueue **queue)
{
    if (*queue != NULL)
        g_queue_free(*queue);

    *queue = g_queue_new();
}

void codec_list_add(codec_t * c, GQueue **queue)
{
    // Add a codec to a specific list
    g_queue_push_tail(*queue, (gpointer) c);
}

void codec_set_active(codec_t *c, gboolean active)
{
    c->is_active = active;
}

guint codec_list_get_size()
{
    // The system wide codec list and the one per account have exactly the same size
    // The only difference may be the order and the enabled codecs
    return g_queue_get_length(codecsCapabilities);
}

codec_t* codec_list_get_by_name(gconstpointer name, GQueue *q)
{
    // If NULL is passed as argument, we look into the global capabilities
    if (q == NULL)
        q = codecsCapabilities;

    GList * c = g_queue_find_custom(q, name, is_name_codecstruct);
    return c ? c->data : NULL;
}

codec_t* codec_list_get_by_payload(gconstpointer payload, GQueue *q)
{
    // If NULL is passed as argument, we look into the global capabilities
    if (q == NULL)
        q = codecsCapabilities;

    GList * c = g_queue_find_custom(q, payload, is_payload_codecstruct);
    return c ? c->data : NULL;
}

codec_t* codec_list_get_nth(guint codec_index, GQueue *q)
{
    return g_queue_peek_nth(q, codec_index);
}

codec_t* capabilities_get_nth(guint caps_index)
{
    return g_queue_peek_nth(codecsCapabilities, caps_index);
}

void codec_set_prefered_order(guint codec_index, GQueue *q)
{
    codec_t * prefered = codec_list_get_nth(codec_index, q);
    g_queue_pop_nth(q, codec_index);
    g_queue_push_head(q, prefered);
}

void codec_list_move_codec_up(guint codec_index, GQueue **q)
{
    DEBUG("Codec list Size: %i \n", codec_list_get_size());

    GQueue *tmp = *q;

    if (codec_index != 0) {
        gpointer codec = g_queue_pop_nth(tmp, codec_index);
        g_queue_push_nth(tmp, codec, codec_index - 1);
    }

    *q = tmp;
}

void codec_list_move_codec_down(guint codec_index, GQueue **q)
{

    DEBUG("Codec list Size: %i \n", codec_list_get_size());

    GQueue *tmp = *q;

    if (codec_index != g_queue_get_length(tmp)) {
        gpointer codec = g_queue_pop_nth(tmp, codec_index);
        g_queue_push_nth(tmp, codec, codec_index + 1);
    }

    *q = tmp;

}

void codec_list_update_to_daemon(const account_t *acc)
{
    // Length of the codec list
    int length = g_queue_get_length(acc->codecs);

    // String listing codecs payloads
    // Initiate double array char list for one string
    const gchar **codecList = (void*) g_malloc(sizeof(void*));

    // Get all codecs in queue
    int c = 0;
    int i;

    for (i = 0; i < length; i++) {
        codec_t* currentCodec = codec_list_get_nth(i, acc->codecs);

        if (currentCodec) {
            // Save only if active
            if (currentCodec->is_active) {
                // Reallocate memory each time more than one active codec is found
                if (c != 0)
                    codecList = (void*) g_realloc(codecList, (c + 1) * sizeof(void*));

                // Allocate memory for the payload
                *(codecList + c) = (gchar*) g_malloc(sizeof(gchar*));
                char payload[10];
                // Put payload string in char array
                sprintf(payload, "%d", currentCodec->_payload);
                strcpy((char*) *(codecList+c), payload);
                c++;
            }
        }
    }

    // Allocate NULL array at the end for Dbus
    codecList = (void*) g_realloc(codecList, (c + 1) * sizeof(void*));
    *(codecList+c) = NULL;

    // call dbus function with array of strings
    dbus_set_active_audio_codec_list(codecList, acc->accountID);

    // Delete memory
    for (i = 0; i < c; i++)
        g_free((gchar*) *(codecList+i));

    g_free(codecList);
}

GQueue* get_system_codec_list(void)
{
    return codecsCapabilities;
}
