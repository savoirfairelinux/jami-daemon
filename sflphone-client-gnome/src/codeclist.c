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
#include <stdint.h>
#include <unistd.h>

#include "dbus.h"

static GQueue audioCodecs = G_QUEUE_INIT;
static GQueue videoCodecs = G_QUEUE_INIT;

/*
 * Instanciate a new codec
 *
 * @param payload       The unique RTP payload
 * @return codec        The new codec instance, or NULL
 */
static codec_t *codec_create (gint payload, gchar **specs)
{
    codec_t *codec = g_new0 (codec_t, 1);
    if (!codec) {
        g_strfreev(specs);
        return NULL;
    }

    codec->payload = payload;
    codec->name = specs[0];
    codec->bitrate = specs[1];
    codec->sample_rate = specs[2];
    codec->is_active = TRUE;

    g_free(specs);

    return codec;
}

static gboolean codecs_audio_load (void)
{
    gchar **codecs = dbus_audio_codec_list();
    gchar **codecs_orig = codecs;

    if (!codecs)
        return FALSE;

    // Add the codecs in the list
    for (; *codecs; codecs++) {
        int payload = atoi(*codecs);
        codec_t *c = codec_create(payload, dbus_audio_codec_details(payload));
        if (c)
            g_queue_push_tail (&audioCodecs, (gpointer*) c);
        g_free(*codecs);
    }
    g_free(codecs_orig);

    // If we didn't load any codecs, problem ...
    if (g_queue_get_length (&audioCodecs) == 0) {
        return FALSE;
    }

    return TRUE;
}

void codec_create_new (gint payload, gboolean active, codec_t **c)
{

    codec_t *codec;
    gchar **specs;

    codec = g_new0 (codec_t, 1);
    codec->payload = payload;
    specs = (gchar **) dbus_audio_codec_details (payload);
    codec->name = specs[0];
    codec->sample_rate = atoi (specs[1]);
    codec->bitrate = atoi (specs[2]);
    codec->is_active = active;

    *c = codec;
}

void codec_create_new_with_specs (gint payload, gchar **specs, gboolean active, codec_t **c)
{

    codec_t *codec;

    codec = g_new0 (codec_t, 1);
    codec->payload = payload;
    codec->name = strdup(specs[0]);
    codec->sample_rate = atoi (specs[1]);
    codec->bitrate = atoi (specs[2]);
    codec->is_active = active;

    *c = codec;
}

static gboolean codecs_video_load (void)
{
    gchar **codecs = dbus_video_codec_list();
    gchar **codecs_orig = codecs;

    if (!codecs)
        return FALSE;

    int payload = 96; // dynamic payloads
    // Add the codecs in the list
    for (; *codecs; codecs++) {
        codec_t *c = codec_create(payload++, dbus_video_codec_details(*codecs));
        if (c)
            g_queue_push_tail (&videoCodecs, (gpointer*) c);
        g_free(*codecs);
    }
    g_free(codecs_orig);

    // If we didn't load any codecs, problem ...
    if (g_queue_get_length (&videoCodecs) == 0) {
        return FALSE;
    }
    return TRUE;
}

gboolean codecs_load(void)
{
    return codecs_audio_load() && codecs_video_load();
}

static void codec_free(gpointer data, gpointer user_data UNUSED)
{
    codec_t *codec = (codec_t*)data;
    g_free(codec->name);
    g_free(codec->sample_rate);
    g_free(codec->bitrate);
}

void codecs_unload (void)
{
    g_queue_foreach(&audioCodecs, codec_free, NULL);
}

codec_t *codec_create_new_from_caps (codec_t *original)
{
    if (!original)
        return NULL;

    codec_t *codec = g_new0 (codec_t, 1);
    if (codec) {
        memcpy(codec, original, sizeof *codec);
        codec->is_active = TRUE;
    }

    return codec;
}

static gint
is_name_codecstruct (gconstpointer a, gconstpointer b)
{
    return strcmp (((codec_t*)a)->name, (const gchar *) b);
}

codec_t* codec_list_get_by_name (gconstpointer name, GQueue *q)
{
    GList * c = g_queue_find_custom (q, name, is_name_codecstruct);
    return c ? (codec_t *) c->data : NULL;
}

static gint
is_payload_codecstruct (gconstpointer a, gconstpointer b)
{
    return ((codec_t*)a)->payload != GPOINTER_TO_INT (b);
}

codec_t* codec_list_get_by_payload (int payload, GQueue *q)
{
    GList * c = g_queue_find_custom (q, (gconstpointer)(uintptr_t)payload, is_payload_codecstruct);
    return c ? (codec_t *) c->data : NULL;
}

void codec_set_prefered_order (guint index, GQueue *q)
{
    codec_t * prefered = g_queue_peek_nth (q, index);
    g_queue_pop_nth (q, index);
    g_queue_push_head (q, prefered);
}

void codec_list_move (guint index, GQueue *q, gboolean up)
{
    guint already = up ? 0 : q->length;
    gint  new = up ? index - 1 : index + 1;

    if (index == already)
        return;

    gpointer codec = g_queue_pop_nth (q, index);
    g_queue_push_nth (q, codec, new);
}

static void codec_list_update_to_daemon_cat (account_t *acc, gboolean is_audio)
{
    gchar** codecList = NULL;

    GQueue *q = is_audio ? acc->codecs : acc->vcodecs;
    int length = q->length;

    // Get all codecs in queue
    int i, c = 0;

    for (i = 0; i < length; i++) {
        codec_t* currentCodec = g_queue_peek_nth (q, i);

        // Save only if active
        if (currentCodec && currentCodec->is_active) {
            codecList = (void*) realloc (codecList, (c+1) *sizeof (void*));

            if (is_audio)
                * (codecList+c) = g_strdup_printf("%d", currentCodec->payload);
            else
                * (codecList+c) = g_strdup(currentCodec->name);
            c++;
        }
    }

    // Allocate NULL array at the end for Dbus
    codecList = (void*) realloc (codecList, (c+1) *sizeof (void*));
    * (codecList+c) = NULL;
    c++;

    // call dbus function with array of strings
    if (is_audio)
        dbus_set_active_audio_codec_list ((const gchar**)codecList, acc->accountID);
    else
        dbus_set_active_video_codec_list ((const gchar**)codecList, acc->accountID);

    // Delete memory
    for (i = 0; i < c; i++)
        free (*(codecList+i));
    free (codecList);
}

void codec_list_update_to_daemon (account_t *acc)
{
    codec_list_update_to_daemon_cat (acc, TRUE);
    codec_list_update_to_daemon_cat (acc, FALSE);
}

GQueue* get_audio_codecs_list (void)
{
    return &audioCodecs;
}

GQueue* get_video_codecs_list (void)
{
    return &videoCodecs;
}
