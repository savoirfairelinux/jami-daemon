/*
 *  Copyright (C) 2004-2014 Savoir-Faire Linux Inc.
 *  Author: Vivien Didelot <vivien.didelot@savoirfairelinux.com>
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

#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../dbus/dbus.h"
#include "video_capabilities.h"

static void append_sizes_and_rates(gpointer key, gpointer value, gpointer data);
static void append_channels_and_sizes(gpointer key, gpointer value, gpointer data);
static gboolean free_node(GNode *node, G_GNUC_UNUSED gpointer data);
static GNode *find_child(GNode *parent, const gchar *data);
static gint alphacmp(gconstpointer p1, gconstpointer p2, gpointer data);
static gint intcmp(gconstpointer p1, gconstpointer p2, gpointer data);
static gchar **children_as_strv(GNode *node, GCompareDataFunc compar);

VideoCapabilities *
video_capabilities_new(const gchar *name)
{
    GNode *root = g_node_new(g_strdup(name));
    GHashTable *hash = dbus_video_get_capabilities(name);
    g_assert(hash);
    g_hash_table_foreach(hash, append_channels_and_sizes, root);
    return root;
}

void
video_capabilities_free(VideoCapabilities *cap)
{
    GNode *root = (GNode *) cap;
    g_node_traverse(root, G_IN_ORDER, G_TRAVERSE_ALL, -1, free_node, NULL);
    g_node_destroy(root);
}

gchar **
video_capabilities_get_channels(VideoCapabilities *cap)
{
    GNode *root = (GNode *) cap;
    return children_as_strv(root, alphacmp);
}

gchar **
video_capabilities_get_sizes(VideoCapabilities *cap, const gchar *channel)
{
    GNode *root = (GNode *) cap;
    GNode *chan_node = find_child(root, channel);
    g_assert(chan_node);
    return children_as_strv(chan_node, intcmp);
}

gchar **
video_capabilities_get_rates(VideoCapabilities *cap, const gchar *channel, const gchar *size)
{
    GNode *root = (GNode *) cap;
    GNode *chan_node = find_child(root, channel);
    g_assert(chan_node);
    GNode *size_node = find_child(chan_node, size);
    g_assert(size_node);
    return children_as_strv(size_node, intcmp);
}

static void
append_sizes_and_rates(gpointer key, gpointer value, gpointer data)
{
    const gchar *size = (gchar *) key;
    const gchar **rates = (const gchar **) value;
    GNode *root = (GNode *) data;
    GNode *node = g_node_append_data(root, g_strdup(size));
    for (gint i = 0; rates[i]; ++i)
        g_node_append_data(node, g_strdup(rates[i]));
}

static void
append_channels_and_sizes(gpointer key, gpointer value, gpointer data)
{
    const gchar *chan = (gchar *) key;
    GHashTable *sizes = (GHashTable *) value;
    GNode *root = (GNode *) data;
    GNode *node = g_node_append_data(root, g_strdup(chan));
    g_hash_table_foreach(sizes, append_sizes_and_rates, node);
}

static gboolean
free_node(GNode *node, G_GNUC_UNUSED gpointer data)
{
    g_free(node->data);
    return FALSE; /* Do not halt the traversal */
}

static GNode *
find_child(GNode *parent, const gchar *data)
{
    GNode *child = g_node_first_child(parent);

    while (child) {
        if (g_strcmp0(child->data, data) == 0)
            break;
        child = g_node_next_sibling(child);
    }

    return child;
}

static gint
alphacmp(gconstpointer p1, gconstpointer p2, G_GNUC_UNUSED gpointer data)
{
    /*
     * The actual arguments to this function are "pointers to pointers to char",
     * but strcmp(3) arguments are "pointers to char", hence the following cast
     * plus dereference.
     */
    return g_strcmp0(* (gchar * const *) p1, * (gchar * const *) p2);
}

static gint
intcmp(gconstpointer p1, gconstpointer p2, G_GNUC_UNUSED gpointer data)
{
    const gint i1 = atoi(* (gchar * const *) p1);
    const gint i2 = atoi(* (gchar * const *) p2);

    return i1 - i2;
}

static gchar **
children_as_strv(GNode *node, GCompareDataFunc compar)
{
    const guint n = g_node_n_children(node);
    gchar *array[n + 1];
    guint i;

    for (i = 0; i < n; ++i)
        array[i] = g_node_nth_child(node, i)->data;
    array[i] = NULL;

    if (compar)
        g_qsort_with_data(array, n, sizeof(gchar *), compar, NULL);

    return g_strdupv(array);
}
