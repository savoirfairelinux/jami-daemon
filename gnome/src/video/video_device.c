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

#include "../dbus/dbus.h"
#include "video_capabilities.h"
#include "video_device.h"

struct video_device {
    VideoCapabilities *cap;

    /* Prefered settings */
    gchar *channel;
    gchar *size;
    gchar *rate;
};

VideoDevice *
video_device_new(const gchar *name)
{
    VideoDevice *dev = g_new(VideoDevice, 1);
    dev->cap = video_capabilities_new(name);
    GHashTable *hash = dbus_get_video_preferences(name);

    /* Store preferences, if any */
    dev->channel = g_strdup(g_hash_table_lookup(hash, "channel"));
    dev->size = g_strdup(g_hash_table_lookup(hash, "size"));
    dev->rate = g_strdup(g_hash_table_lookup(hash, "rate"));

    g_hash_table_destroy(hash);

    return dev;
}

void
video_device_free(VideoDevice *dev)
{
    video_capabilities_free(dev->cap);
    g_free(dev->channel);
    g_free(dev->size);
    g_free(dev->rate);
    g_free(dev);
}

gchar **
video_device_get_channels(VideoDevice *dev)
{
    return video_capabilities_get_channels(dev->cap);
}

gchar **
video_device_get_sizes(VideoDevice *dev, const gchar *channel)
{
    return video_capabilities_get_sizes(dev->cap, channel);
}

gchar **
video_device_get_rates(VideoDevice *dev, const gchar *channel, const gchar *size)
{
    return video_capabilities_get_rates(dev->cap, channel, size);
}

gchar *
video_device_get_prefered_channel(VideoDevice *dev)
{
    return dev->channel;
}

gchar *
video_device_get_prefered_size(VideoDevice *dev)
{
    return dev->size;
}

gchar *
video_device_get_prefered_rate(VideoDevice *dev)
{
    return dev->rate;
}

void
video_device_set_prefered_channel(VideoDevice *dev, const gchar *channel)
{
    g_free(dev->channel);
    dev->channel = g_strdup(channel);
}

void
video_device_set_prefered_size(VideoDevice *dev, const gchar *size)
{
    g_free(dev->size);
    dev->size = g_strdup(size);
}

void
video_device_set_prefered_rate(VideoDevice *dev, const gchar *rate)
{
    g_free(dev->rate);
    dev->rate = g_strdup(rate);
}

void
video_device_save_preferences(VideoDevice *dev)
{
    dbus_set_video_preferences(dev->cap->data, dev->channel, dev->size, dev->rate);
}
