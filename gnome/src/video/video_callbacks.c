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
#include "video_renderer.h"

#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>

#include <string.h>
#include "config/videoconf.h"

typedef struct {
    gchar *id;
    GtkWidget *window;
    gboolean fullscreen;
} VideoHandle;

static GHashTable *video_handles;

static gboolean
video_is_local(const gchar *id)
{
    static const gchar * const LOCAL_VIDEO_ID = "local";
    return g_strcmp0(id, LOCAL_VIDEO_ID) == 0;
}

static void
cleanup_handle(gpointer data)
{
    VideoHandle *h = (VideoHandle *) data;
    if (!h)
        return;

    if (GTK_IS_WIDGET(h->window)) {
        gtk_widget_destroy(h->window);
        if (video_is_local(h->id))
            update_camera_button_label();
        g_free(h->id);
    }
    g_free(h);
}

static void
video_window_deleted_cb(G_GNUC_UNUSED GtkWidget *widget, G_GNUC_UNUSED gpointer data)
{
	if (dbus_has_video_camera_started())
		dbus_stop_video_camera();
}

static void
video_window_button_cb(GtkWindow *win, GdkEventButton *event, gpointer data)
{
    VideoHandle *handle = (VideoHandle *) data;
    if (event->type == GDK_2BUTTON_PRESS) {
        g_debug("TOGGLING FULL SCREEEN!");
        handle->fullscreen = !handle->fullscreen;
        if (handle->fullscreen)
            gtk_window_fullscreen(win);
        else
            gtk_window_unfullscreen(win);
    }
}


static VideoHandle*
add_handle(const gchar *id)
{
    if (!video_handles)
        video_handles = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, cleanup_handle);
    if (g_hash_table_lookup(video_handles, id)) {
        g_warning("Already created handle for video with id %s", id);
        return NULL;
    }

    VideoHandle *handle = g_new0(VideoHandle, 1);
    handle->id = g_strdup(id);
    handle->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    handle->fullscreen = FALSE;
    g_signal_connect(handle->window, "button_press_event",
            G_CALLBACK(video_window_button_cb),
            handle);
    g_signal_connect(handle->window, "delete-event",
            G_CALLBACK(video_window_deleted_cb),
            NULL);
    if (video_is_local(id))
        update_camera_button_label();

    g_hash_table_insert(video_handles, g_strdup(id), handle);
    return handle;
}

void video_cleanup()
{
    if (video_handles) {
        g_hash_table_destroy(video_handles);
        video_handles = NULL;
    }
}

static gboolean
try_clutter_init()
{
#define PRINT_ERR(X) \
    case (X): \
    g_warning("%s", #X); \
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

void started_decoding_video_cb(G_GNUC_UNUSED DBusGProxy *proxy,
        gchar *id, gchar *shm_path, gint width, gint height,
        G_GNUC_UNUSED GError *error, G_GNUC_UNUSED gpointer userdata)
{
    if (!id || !*id || !shm_path || !*shm_path)
        return;

    if (!try_clutter_init())
        return;
    VideoHandle *handle = add_handle(id);
    if (!handle)
        return;

    GtkWidget *video_area = gtk_clutter_embed_new();
    ClutterActor *stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(video_area));
    if (!stage) {
        gtk_widget_destroy(video_area);
    } else {
        ClutterColor stage_color = { 0x61, 0x64, 0x8c, 0xff };
        clutter_actor_set_background_color(CLUTTER_ACTOR(stage), &stage_color);
    }

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(vbox), video_area);

    if (video_is_local(id))
        gtk_widget_set_size_request(video_area, 200, 150);
    else
        gtk_widget_set_size_request(video_area, width, height);

    if (handle) {
        gtk_container_add(GTK_CONTAINER(handle->window), vbox);
        gtk_widget_show_all(handle->window);
    }

    g_debug("Video started for id: %s shm-path:%s width:%d height:%d",
           id, shm_path, width, height);

    VideoRenderer *renderer = video_renderer_new(video_area, width, height, shm_path);
    if (!video_renderer_run(renderer)) {
        g_object_unref(renderer);
        g_warning("Could not run video renderer");
        g_hash_table_remove(video_handles, id);
        return;
    }
}

void
stopped_decoding_video_cb(G_GNUC_UNUSED DBusGProxy *proxy,
                          gchar *id,
                          G_GNUC_UNUSED gchar *shm_path,
                          G_GNUC_UNUSED GError *error,
                          G_GNUC_UNUSED gpointer userdata)
{
    if (video_handles)
        g_hash_table_remove(video_handles, id);
}
