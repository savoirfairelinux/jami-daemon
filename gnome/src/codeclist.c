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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "codeclist.h"
#include "unused.h"
#include "logger.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "dbus.h"

static GQueue audioCodecs = G_QUEUE_INIT;
static GQueue videoCodecs = G_QUEUE_INIT;

/*
 * Instantiate a new codec
 *
 * @param payload       The unique RTP payload
 * @return codec        The new codec instance, or NULL
 */
static codec_t *codec_create(gint payload, gchar **specs)
{
    codec_t *codec = g_new0(codec_t, 1);
    if (!codec) {
        g_strfreev(specs);
        return NULL;
    }

    codec->payload = payload;
    codec->name = specs[0];
    codec->bitrate = specs[1];
    codec->sample_rate = specs[2] ? atoi(specs[2]) : 0;
    codec->is_active = TRUE;

    free(specs[2]);
    free(specs);

    return codec;
}

static gboolean codecs_audio_load(void)
{
    // This is a global list inherited by all accounts
    GArray *codecs = dbus_audio_codec_list();

    // Add the codecs in the list
    for (guint i = 0; i < codecs->len; i++) {
        gint payload = g_array_index(codecs, gint, i);
        codec_t *c = codec_create(payload, dbus_audio_codec_details(payload));
        if (c)
            g_queue_push_tail(&audioCodecs, (gpointer*) c);
    }

    g_array_unref(codecs);

    // If we didn't load any codecs, problem ...
    return g_queue_get_length(&audioCodecs) > 0;
}

#ifdef SFL_VIDEO
static gboolean codecs_video_load(void)
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
            g_queue_push_tail(&videoCodecs, (gpointer*) c);
        g_free(*codecs);
    }
    g_free(codecs_orig);

    // If we didn't load any codecs, problem ...
    return g_queue_get_length(&videoCodecs) > 0;
}

gboolean codecs_load(void)
{
    return codecs_audio_load() && codecs_video_load();
}
#else
gboolean codecs_load(void)
{
    return codecs_audio_load();
}
#endif

static void codec_free(gpointer data, gpointer user_data UNUSED)
{
    codec_t *codec = (codec_t*) data;
    g_free(codec->name);
    g_free(codec->bitrate);
}

void codecs_unload(void)
{
    g_queue_foreach(&audioCodecs, codec_free, NULL);
}

codec_t *codec_create_new_from_caps(codec_t *original)
{
    if (!original)
        return NULL;

    codec_t *codec = g_new0(codec_t, 1);
    if (codec) {
        memcpy(codec, original, sizeof *codec);
        codec->is_active = TRUE;
    }

    return codec;
}

static gint
is_name_codecstruct(gconstpointer a, gconstpointer b)
{
    return g_strcmp0(((codec_t*) a)->name, (const gchar *) b);
}

codec_t* codec_list_get_by_name(gconstpointer name, GQueue *q)
{
    GList * c = g_queue_find_custom(q, name, is_name_codecstruct);
    return c ? (codec_t *) c->data : NULL;
}

static gint
is_payload_codecstruct(gconstpointer a, gconstpointer b)
{
    const codec_t *c = a;
    return (c->payload == GPOINTER_TO_INT(b)) ? 0 : 1;
}

codec_t* codec_list_get_by_payload(int payload, GQueue *q)
{
    GList * c = g_queue_find_custom(q, (gconstpointer)(uintptr_t)payload, is_payload_codecstruct);
    return c ? c->data : NULL;
}

void codec_set_prefered_order(guint codec_index, GQueue *q)
{
    codec_t * prefered = (codec_t *) g_queue_peek_nth(q, codec_index);
    g_queue_pop_nth(q, codec_index);
    g_queue_push_head(q, prefered);
}

void codec_list_move(guint codec_index, GQueue *q, gboolean up)
{
    guint already = up ? 0 : q->length;
    gint  new = up ? codec_index - 1 : codec_index + 1;

    if (codec_index == already)
        return;

    gpointer codec = g_queue_pop_nth(q, codec_index);
    g_queue_push_nth(q, codec, new);
}

/* FIXME:tmatth: Clean this up, shouldn't have to do all the reallocs
 * explicitly if we use a nicer data structure */
static void
codec_list_update_to_daemon_audio(account_t *acc)
{
    guint c = 0;

    gchar** codecList = NULL;
    // Get all codecs in queue
    for (guint i = 0; i < acc->acodecs->length; i++) {
        codec_t* currentCodec = g_queue_peek_nth(acc->acodecs, i);

        // Save only if active
        if (currentCodec && currentCodec->is_active) {
            codecList = (void *) g_realloc(codecList, (c + 1) * sizeof(void *));
            *(codecList + c) = g_strdup_printf("%d", currentCodec->payload);
            c++;
        }
    }

    // Allocate NULL array at the end for Dbus
    codecList = (void *) g_realloc(codecList, (c + 1) * sizeof(void*));
    *(codecList + c) = NULL;
    c++;

    // call dbus function with array of strings
    dbus_set_active_audio_codec_list((const gchar**) codecList, acc->accountID);
    // Delete memory
    for (guint i = 0; i < c; i++)
        g_free(*(codecList + i));
    g_free(codecList);
}

#ifdef SFL_VIDEO
static void codec_list_update_to_daemon_video(account_t *acc)
{
    gchar** codecList = NULL;
    // Get all codecs in queue
    guint c = 0;
    for (guint i = 0; i < acc->vcodecs->length; i++) {
        codec_t* currentCodec = g_queue_peek_nth(acc->vcodecs, i);

        // Save only if active
        if (currentCodec && currentCodec->is_active) {
            codecList = (void *) g_realloc(codecList, (c + 1) * sizeof(void *));
            *(codecList + c) = g_strdup(currentCodec->name);
            c++;
        }
    }

    // Allocate NULL array at the end for Dbus
    codecList = (void*) g_realloc(codecList, (c + 1) * sizeof (void*));
    *(codecList + c) = NULL;
    c++;

    // call dbus function with array of strings
    dbus_set_active_video_codec_list((const gchar**) codecList, acc->accountID);

    // Delete memory
    for (guint i = 0; i < c; i++)
        g_free(*(codecList + i));
    g_free(codecList);
}
#endif

void codec_list_update_to_daemon(account_t *acc)
{
    codec_list_update_to_daemon_audio(acc);
#ifdef SFL_VIDEO
    codec_list_update_to_daemon_video(acc);
#endif
}

GQueue* get_audio_codecs_list(void)
{
    return &audioCodecs;
}

GQueue* get_video_codecs_list(void)
{
    return &videoCodecs;
}
