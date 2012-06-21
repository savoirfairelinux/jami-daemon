/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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

#include "video_callbacks.h"
#include "video_renderer.h"

#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>

#include "config/videoconf.h"
#include <string.h>
#include "actions.h"
#include "logger.h"
#include "unused.h"

// FIXME: get rid of these
static GtkWidget *video_window_global = NULL;
static gboolean video_window_fullscreen = FALSE;

static gboolean
video_stream_is_local(const gchar * id)
{
    static const gchar * const LOCAL_VIDEO_ID = "local";
    return g_strcmp0(id, LOCAL_VIDEO_ID) == 0;
}

static void
video_window_deleted_cb(GtkWidget *widget UNUSED, gpointer data UNUSED)
{
    // FIXME: probably need to do something smarter here
    sflphone_hang_up();
}

static void
video_window_button_cb(GtkWindow *win, GdkEventButton *event,
                                 gpointer fullscreen)
{
    int *fs = fullscreen;
    if (event->type == GDK_2BUTTON_PRESS) {
        *fs = !*fs;
        if (*fs)
            gtk_window_fullscreen(win);
        else
            gtk_window_unfullscreen(win);
    }
}

static gboolean
try_clutter_init()
{
#define PRINT_ERR(X) \
    case (X): \
    ERROR("%s", #X); \
    break;

    switch (gtk_clutter_init(NULL, NULL)) {
        case CLUTTER_INIT_SUCCESS:
            return TRUE;
        PRINT_ERR(CLUTTER_INIT_ERROR_UNKNOWN);
        PRINT_ERR(CLUTTER_INIT_ERROR_THREADS);
        PRINT_ERR(CLUTTER_INIT_ERROR_BACKEND);
        PRINT_ERR(CLUTTER_INIT_ERROR_INTERNAL);
    }
    return FALSE;
#undef PRINT_ERR
}

void started_decoding_video_cb(DBusGProxy *proxy UNUSED,
        gchar *id, gchar *shm_path, gint width, gint height,
        GError *error UNUSED, gpointer userdata UNUSED)
{
    if (!video_window_global) {
        video_window_global = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        video_window_fullscreen = FALSE;
        if (video_stream_is_local(id))
            toggle_preview_button_label();
        g_signal_connect(video_window_global, "button_press_event",
                         G_CALLBACK(video_window_button_cb),
                         &video_window_fullscreen);
        g_signal_connect(video_window_global, "delete-event",
                         G_CALLBACK(video_window_deleted_cb), NULL);
    }

    if (!try_clutter_init())
        return;

    GtkWidget *video_area = gtk_clutter_embed_new();
    ClutterActor *stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(video_area));
    if (!stage)
        gtk_widget_destroy(video_area);
    else {
        ClutterColor stage_color = { 0x61, 0x64, 0x8c, 0xff };
        clutter_stage_set_color(CLUTTER_STAGE(stage), &stage_color);
    }

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(vbox), video_area);

    if (shm_path == 0 || strlen(shm_path) == 0)
        return;

    gtk_widget_set_size_request(video_area, width, height);
    if (video_window_global) {
        gtk_container_add(GTK_CONTAINER(video_window_global), vbox);
        gtk_widget_show_all(video_window_global);
    }

    DEBUG("Video started for id: %s shm-path:%s width:%d height:%d",
           id, shm_path, width, height);

    VideoRenderer *renderer = video_renderer_new(video_area, width, height, shm_path);
    if (!video_renderer_run(renderer)) {
        g_object_unref(renderer);
        ERROR("Could not run video renderer");
        return;
    }
}

void
stopped_decoding_video_cb(DBusGProxy *proxy UNUSED, gchar *id, gchar *shm_path, GError *error UNUSED, gpointer userdata UNUSED)
{
    DEBUG("Video stopped for id %s, shm path %s", id, shm_path);

    if (video_window_global) {
        if (GTK_IS_WIDGET(video_window_global))
            gtk_widget_destroy(video_window_global);
        video_window_global = NULL;
    }
}
