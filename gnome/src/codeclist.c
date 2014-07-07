/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "codeclist.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>

#include "dbus.h"

static GQueue audioCodecs = G_QUEUE_INIT;

/*
 * Instantiate a new codec
 *
 * @param payload       The unique RTP payload
 * @return codec        The new codec instance, or NULL
 */
/* FIXME: use GHashTable instead of list of details */
static codec_t *codec_create(gint payload, gchar **specs)
{
    codec_t *codec = g_new0(codec_t, 1);
    codec->payload = payload;
    codec->name = g_strdup(specs[0]);
    if (specs[1] && specs[2]) {
        codec->bitrate = g_strdup(specs[2]);
        codec->sample_rate = atoi(specs[1]);
    }
    codec->channels = specs[3]?atoi(specs[3]):1;
    codec->is_active = TRUE;

    g_strfreev(specs);

    return codec;
}

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
    return (c->payload == GPOINTER_TO_INT(b)) ? 0 : 1;
}

static gboolean codecs_audio_load(void)
{
    // This is a global list inherited by all accounts
    GArray *codecs = dbus_audio_codec_list();

    // Add the codecs in the list
    for (guint i = 0; i < codecs->len; i++) {
        gint payload = g_array_index(codecs, gint, i);
        codec_t *c = codec_create(payload, dbus_audio_codec_details(payload));
        g_queue_push_tail(&audioCodecs, (gpointer*) c);
    }

    g_array_unref(codecs);

    // If we didn't load any codecs, problem ...
    return g_queue_get_length(&audioCodecs) > 0;
}

static void
codec_free(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
    codec_t *codec = (codec_t*) data;
    g_free(codec->name);
    g_free(codec->bitrate);
}

static void codecs_audio_unload(void)
{
    g_queue_foreach(&audioCodecs, codec_free, NULL);
}

gboolean codecs_load(void)
{
    return codecs_audio_load();
}

void codecs_unload(void)
{
    codecs_audio_unload();
}

codec_t *codec_create_new_from_caps(codec_t *original)
{
    codec_t *codec = NULL;

    if (original) {
        codec = g_new0(codec_t, 1);
        codec->payload = original->payload;
        codec->is_active = original->is_active;
        codec->name = g_strdup(original->name);
        codec->sample_rate = original->sample_rate;
        codec->bitrate = g_strdup(original->bitrate);
        codec->channels = original->channels;
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

codec_t* codec_list_get_by_name(gconstpointer name, GQueue *q)
{
    GList * c = g_queue_find_custom(q, name, is_name_codecstruct);
    return c ? c->data : NULL;
}

codec_t* codec_list_get_by_payload(int payload, GQueue *q)
{
    GList * c = g_queue_find_custom(q, (gconstpointer)(uintptr_t) payload, is_payload_codecstruct);
    return c ? c->data : NULL;
}

codec_t* codec_list_get_nth(guint codec_index, GQueue *q)
{
    return g_queue_peek_nth(q, codec_index);
}

void codec_set_preferred_order(guint codec_index, GQueue *q)
{
    codec_t * preferred = codec_list_get_nth(codec_index, q);
    g_queue_pop_nth(q, codec_index);
    g_queue_push_head(q, preferred);
}

void codec_list_move_codec_up(guint codec_index, GQueue **q)
{
    GQueue *tmp = *q;

    if (codec_index != 0) {
        gpointer codec = g_queue_pop_nth(tmp, codec_index);
        g_queue_push_nth(tmp, codec, codec_index - 1);
    }

    *q = tmp;
}

void codec_list_move_codec_down(guint codec_index, GQueue **q)
{
    GQueue *tmp = *q;

    if (codec_index != g_queue_get_length(tmp)) {
        gpointer codec = g_queue_pop_nth(tmp, codec_index);
        g_queue_push_nth(tmp, codec, codec_index + 1);
    }

    *q = tmp;
}

/* Returns a list of strings for just the active codecs in a given queue of codecs */
static GSList*
codec_list_get_active_codecs(GQueue *codecs)
{
    GSList *active = NULL;
    for (guint i = 0; i < codecs->length; i++) {
        codec_t* currentCodec = g_queue_peek_nth(codecs, i);
        if (currentCodec && currentCodec->is_active)
            active = g_slist_append(active, g_strdup_printf("%d", currentCodec->payload));
    }
    return active;
}

/* Given a singly linked list of codecs, returns a list of pointers
 * to each element in the list's data. No duplication is done so
 * the returned list is only valid for the lifetime of the GSList */
static gchar **
get_items_from_list(GSList *codecs)
{
    const guint length = g_slist_length(codecs);
    /* we add +1 because the last element must be a NULL pointer for d-bus */
    gchar **activeCodecsStr = g_new0(gchar*, length + 1);
    for (guint i = 0; i < length; ++i)
        activeCodecsStr[i] = g_slist_nth_data(codecs, i);
    return activeCodecsStr;
}

static void
codec_list_update_to_daemon_audio(const account_t *acc)
{
    GSList *activeCodecs = codec_list_get_active_codecs(acc->acodecs);
    gchar **activeCodecsStr = get_items_from_list(activeCodecs);

    // call dbus function with array of strings
    dbus_set_active_audio_codec_list((const gchar **) activeCodecsStr, acc->accountID);
    g_free(activeCodecsStr);
    g_slist_free_full(activeCodecs, g_free);
}

void codec_list_update_to_daemon(const account_t *acc)
{
    codec_list_update_to_daemon_audio(acc);
}

GQueue* get_audio_codecs_list(void)
{
    return &audioCodecs;
}
