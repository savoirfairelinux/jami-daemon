/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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

#include "video_callbacks.h"
#include "video_widget.h"

static gboolean
video_is_local(const gchar *id)
{
    static const gchar * const LOCAL_VIDEO_ID = "local";

    return g_strcmp0(id, LOCAL_VIDEO_ID) == 0;
}

void
started_decoding_video_cb(G_GNUC_UNUSED DBusGProxy *proxy,
                          gchar *id,
                          gchar *shm_path,
                          gint width,
                          gint height,
                          gboolean is_mixer,
                          gpointer userdata)
{
    if (!id || !*id || !shm_path || !*shm_path)
        return;

    SFLPhoneClient * client = userdata;

    video_widget_camera_start(client->video, video_is_local(id) ?
            VIDEO_AREA_LOCAL : VIDEO_AREA_REMOTE, id, shm_path, width, height,
            is_mixer);

}

void
stopped_decoding_video_cb(G_GNUC_UNUSED DBusGProxy *proxy,
                          gchar *id,
                          G_GNUC_UNUSED gchar *shm_path,
                          G_GNUC_UNUSED gboolean is_mixer,
                          gpointer userdata)
{
    SFLPhoneClient * client = userdata;

    video_widget_camera_stop(client->video, video_is_local(id) ?
            VIDEO_AREA_LOCAL : VIDEO_AREA_REMOTE, id);
}
