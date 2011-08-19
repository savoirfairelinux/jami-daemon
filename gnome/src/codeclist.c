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

#include <codeclist.h>

#include <string.h>
#include <stdlib.h>
#include <unistd.h>

#include "dbus.h"

static GQueue * codecsCapabilities = NULL;

gint
is_name_codecstruct (gconstpointer a, gconstpointer b)
{
    codec_t * c = (codec_t *) a;

    if (g_strcmp0 (c->name, (const gchar *) b) == 0)
        return 0;
    else
        return 1;
}

gint
is_payload_codecstruct (gconstpointer a, gconstpointer b)
{
    codec_t * c = (codec_t *) a;

    if (c->_payload == GPOINTER_TO_INT (b))
        return 0;
    else
        return 1;
}

void codec_capabilities_load (void)
{
    // Create the queue object that will contain the global list of audio codecs
    if (codecsCapabilities != NULL)
        g_queue_free (codecsCapabilities);

    codecsCapabilities = g_queue_new();

    // This is a global list inherited by all accounts
    gchar **codecs = dbus_audio_codec_list ();
    gchar **codecs_orig = codecs;

    if (codecs != NULL) {
        // Add the codecs in the list
        for (; *codecs; codecs++) {
            codec_t *c;
            guint payload = atoi (*codecs);
            gchar **specs = dbus_audio_codec_details (payload);
            codec_create_new_with_specs (payload, specs, TRUE, &c);
            g_strfreev(specs);
            g_queue_push_tail (codecsCapabilities, (gpointer) c);
            g_free(*codecs);
        }
        g_free(codecs_orig);
    }

    // If we didn't load any codecs, problem ...
    if (g_queue_get_length (codecsCapabilities) == 0) {
        // Error message
        ERROR ("No audio codecs found");
        dbus_unregister (getpid());
        exit (1);
    }
}

void account_create_codec_list (account_t **acc)
{
    GQueue *_codecs;

    _codecs = (*acc)->codecs;

    if (_codecs != NULL)
        g_queue_free (_codecs);

    _codecs = g_queue_new ();

    (*acc)->codecs = _codecs;
}

void codec_create_new (gint payload, gboolean active, codec_t **c)
{
    codec_t *codec;
    gchar **specs;

    codec = g_new0 (codec_t, 1);
    codec->_payload = payload;
    specs = (gchar **) dbus_audio_codec_details (payload);
    codec->name = specs[0];
    codec->sample_rate = atoi (specs[1]);
    codec->_bitrate = atoi (specs[2]);
    codec->is_active = active;

    *c = codec;
}

void codec_create_new_with_specs (gint payload, gchar **specs, gboolean active, codec_t **c)
{
    codec_t *codec;

    codec = g_new0 (codec_t, 1);
    codec->_payload = payload;
    codec->name = strdup(specs[0]);
    codec->sample_rate = atoi (specs[1]);
    codec->_bitrate = atoi (specs[2]);
    codec->is_active = active;

    *c = codec;
}

void codec_create_new_from_caps (codec_t *original, codec_t **copy)
{
    codec_t *codec;

    if (!original) {
        *copy = NULL;
        return;
    }

    codec = g_new0 (codec_t, 1);
    codec->_payload = original->_payload;
    codec->name = original->name;
    codec->sample_rate = original->sample_rate;
    codec->_bitrate = original->_bitrate;
    codec->is_active = original->is_active;

    *copy = codec;
}


void codec_list_clear (GQueue **queue)
{
    if (*queue != NULL)
        g_queue_free (*queue);
    *queue = g_queue_new();
}

void codec_list_add (codec_t * c, GQueue **queue)
{
    // Add a codec to a specific list
    g_queue_push_tail (*queue, (gpointer) c);
}

void codec_set_active (codec_t **c)
{
    if (c) {
        DEBUG ("%s set active", (*c)->name);
        (*c)->is_active = TRUE;
    }
}

void codec_set_inactive (codec_t **c)
{
    if (c) {
        DEBUG ("%s set inactive", (*c)->name);
        (*c)->is_active = FALSE;
    }
}

guint codec_list_get_size ()
{
    // The system wide codec list and the one per account have exactly the same size
    // The only difference may be the order and the enabled codecs
    return g_queue_get_length (codecsCapabilities);
}

codec_t* codec_list_get_by_name (gconstpointer name, GQueue *q)
{
    // If NULL is passed as argument, we look into the global capabilities
    if (q == NULL)
        q = codecsCapabilities;

    GList * c = g_queue_find_custom (q, name, is_name_codecstruct);

    if (c)
        return (codec_t *) c->data;
    else
        return NULL;
}

codec_t* codec_list_get_by_payload (gconstpointer payload, GQueue *q)
{
    // If NULL is passed as argument, we look into the global capabilities
    if (q == NULL)
        q = codecsCapabilities;

    GList * c = g_queue_find_custom (q, payload, is_payload_codecstruct);

    if (c)
        return (codec_t *) c->data;
    else
        return NULL;
}

codec_t* codec_list_get_nth (guint index, GQueue *q)
{
    return g_queue_peek_nth (q, index);
}

codec_t* capabilities_get_nth (guint index)
{
    return g_queue_peek_nth (codecsCapabilities, index);
}

void codec_set_prefered_order (guint index, GQueue *q)
{
    codec_t * prefered = codec_list_get_nth (index, q);
    g_queue_pop_nth (q, index);
    g_queue_push_head (q, prefered);
}

void codec_list_move_codec_up (guint index, GQueue **q)
{
    DEBUG ("Codec list Size: %i \n", codec_list_get_size ());

    GQueue *tmp = *q;

    if (index != 0) {
        gpointer codec = g_queue_pop_nth (tmp, index);
        g_queue_push_nth (tmp, codec, index-1);
    }

    *q = tmp;
}

void codec_list_move_codec_down (guint index, GQueue **q)
{

    DEBUG ("Codec list Size: %i \n", codec_list_get_size());

    GQueue *tmp = *q;

    if (index != g_queue_get_length(tmp) ) {
        gpointer codec = g_queue_pop_nth (tmp, index);
        g_queue_push_nth (tmp, codec, index+1);
    }

    *q = tmp;

}

void codec_list_update_to_daemon (account_t *acc)
{
    // Length of the codec list
    int length = g_queue_get_length(acc->codecs);

    // String listing codecs payloads
    // Initiate double array char list for one string
    const gchar **codecList = (void*) g_malloc (sizeof (void*));

    // Get all codecs in queue
    int c = 0;
    int i;

    for (i = 0; i < length; i++) {
        codec_t* currentCodec = codec_list_get_nth (i, acc->codecs);

        if (currentCodec) {
            // Save only if active
            if (currentCodec->is_active) {
                // Reallocate memory each time more than one active codec is found
                if (c != 0)
                    codecList = (void*) g_realloc (codecList, (c + 1) * sizeof (void*));

                // Allocate memory for the payload
                * (codecList + c) = (gchar*) g_malloc (sizeof (gchar*));
                char payload[10];
                // Put payload string in char array
                sprintf (payload, "%d", currentCodec->_payload);
                strcpy ( (char*) * (codecList+c), payload);
                c++;
            }
        }
    }

    // Allocate NULL array at the end for Dbus
    codecList = (void*) g_realloc (codecList, (c + 1) * sizeof (void*));
    * (codecList+c) = NULL;

    // call dbus function with array of strings
    dbus_set_active_audio_codec_list (codecList, acc->accountID);

    // Delete memory
    for (i = 0; i < c; i++)
        g_free ( (gchar*) * (codecList+i));

    g_free (codecList);
}

GQueue* get_system_codec_list (void)
{
    return codecsCapabilities;
}
