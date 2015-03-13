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

/*static GQueue audioCodecs = G_QUEUE_INIT;
static GQueue videoCodecs = G_QUEUE_INIT;*/
static GQueue allCodecs = G_QUEUE_INIT;

/*
 * Instantiate a new codec
 *
 * @param codecId       The unique codec identifier
 * @return codec        The new codec instance, or NULL
 */
static codec_t *codec_create(guint codecId, GHashTable* specs)
{
    codec_t *codec = g_new0(codec_t, 1);
    codec->codecId = codecId;
    codec->name = g_hash_table_lookup(specs, CODEC_INFO_NAME);
    codec->type = g_hash_table_lookup(specs, CODEC_INFO_TYPE);
    codec->sample_rate =
        (g_hash_table_lookup(specs, CODEC_INFO_SAMPLE_RATE) ? atoi(g_hash_table_lookup(specs, CODEC_INFO_SAMPLE_RATE)) : 0);
    codec->frame_rate =
        (g_hash_table_lookup(specs, CODEC_INFO_FRAME_RATE) ? atoi(g_hash_table_lookup(specs, CODEC_INFO_FRAME_RATE)) : 0);
    codec->bitrate =
        (g_hash_table_lookup(specs, CODEC_INFO_BITRATE) ? atoi(g_hash_table_lookup(specs, CODEC_INFO_BITRATE)) : 0);
    codec->channels =
        (g_hash_table_lookup(specs, CODEC_INFO_CHANNEL_NUMBER) ? atoi(g_hash_table_lookup(specs, CODEC_INFO_CHANNEL_NUMBER)) : 0);
    codec->is_active = TRUE;
    return codec;
}

gint
is_name_codecstruct(gconstpointer a, gconstpointer b)
{
    const codec_t *c = a;
    return !!g_strcmp0(c->name, (const gchar *) b);
}

static gint
is_codecId_codecstruct(gconstpointer a, gconstpointer b)
{
    const codec_t *c = a;
    return (c->codecId == GPOINTER_TO_INT(b)) ? 0 : 1;
}

static gboolean all_codecs_load(void)
{
    // This is a global list inherited by all accounts
    GArray *codecs = dbus_get_codec_list();

    // Add the codecs in the list
    for (guint i = 0; i < codecs->len; i++) {
        guint codecId = g_array_index(codecs, guint, i);
        codec_t *c = codec_create(codecId, dbus_get_codec_details("IP2IP", codecId));
        g_queue_push_tail(&allCodecs, (gpointer*) c);
    }

    g_array_unref(codecs);

    // If we didn't load any codecs, problem ...
    return (g_queue_get_length(&allCodecs) > 0);
}

static void
codec_free(gpointer data, G_GNUC_UNUSED gpointer user_data)
{
    codec_t *codec = (codec_t*) data;
    if (codec) {
        g_free(codec->name);
        g_free(codec->type);
    }
}

static void all_codecs_unload(void)
{
    g_queue_foreach(&allCodecs, codec_free, NULL);
}

gboolean codecs_load(void)
{
    return all_codecs_load();
}

void codecs_unload(void)
{
    all_codecs_unload();
}

codec_t *codec_create_new_from_caps(codec_t *original)
{
    codec_t *codec = NULL;

    if (original) {
        codec = g_new0(codec_t, 1);
        codec->codecId = original->codecId;
        codec->name = g_strdup(original->name);
        codec->type = g_strdup(original->type);
        codec->payload = original->payload;
        codec->is_active = original->is_active;
        codec->sample_rate = original->sample_rate;
        codec->frame_rate = original->frame_rate;
        codec->bitrate = original->bitrate;
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
codec_t* codec_list_get_by_name_and_sample_rate(gconstpointer name,
        gconstpointer samplerate, GQueue *q)
{
    GList * list = g_queue_peek_head_link (q);
    guint samplerateInt = (samplerate ? atoi(samplerate) * 1000 : 0);

    while (list != NULL) {
        codec_t* codec = (codec_t*) list->data;
        if (g_strcmp0(codec->name, (gchar*) name) == 0
            && (codec->sample_rate == samplerateInt))
            return codec;
        list = list->next;
    }
    return NULL;
}

codec_t* codec_list_get_by_codecId(guint codecId, GQueue *q)
{
    GList * c = g_queue_find_custom(q, (gconstpointer)(uintptr_t) codecId, is_codecId_codecstruct);
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
        codec_t* codecInfo = (codec_t*) codec;
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
static GArray*
codec_list_get_active_codecs(GQueue *codecs)
{
    GArray *active = g_array_new (FALSE, FALSE, sizeof(guint));
    for (guint i = 0; i < codecs->length; i++) {
        codec_t* currentCodec = g_queue_peek_nth(codecs, i);
        if (currentCodec && currentCodec->is_active) {
            active = g_array_append_val(active, currentCodec->codecId);
        }
    }
    return active;
}


static void
codec_list_update_to_daemon_all(const account_t *acc)
{
    GArray* list = codec_list_get_active_codecs(acc->allCodecs);
    if (list)
            dbus_set_active_codec_list(
                acc->accountID
                ,list
                );
}

void codec_list_update_to_daemon(const account_t *acc)
{
    codec_list_update_to_daemon_all(acc);
}

GQueue* get_all_codecs_list(void)
{
    return &allCodecs;
}
