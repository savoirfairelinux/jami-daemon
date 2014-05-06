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
#include "sflphone_client.h"    /* gsettings schema path */
#include "config/videoconf.h"

#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>

#include <glib/gi18n.h>

typedef enum {
    IS_CALL,
    IS_CONFERENCE,
    IS_DESTROYED
} call_type_t;

typedef struct {
    gchar *id;
    GtkWidget *window;
    GSettings *settings;
    gboolean fullscreen;
} VideoHandle;

static GHashTable *video_handles;

/* FIXME: this solution is a workaround, we must have this information from the
 * daemon by receiving a dbus signal, or a new structure to handle ID */
/* This function is an helper function to determine if the ID is a callID or a
 * confID */
static call_type_t
is_call_or_conf(const gchar *id)
{
    /* get the call list and compare the id with all id in the list */
    gchar **call_list = dbus_get_call_list();
    for (gchar **call = call_list; call && *call; call++) {
        /* if we found the same id, it's a call ID */
        if (g_strcmp0(id, *call) == 0) {
            g_strfreev(call_list);
            return IS_CALL;
        }
    }
    g_strfreev(call_list);

    /* get the conference list and compare the id with all id in the list */
    gchar **conference_list = dbus_get_conference_list();
    for (gchar **conference = conference_list; conference && *conference; conference++) {
        /* if we found the same id, it's a conf ID */
        if (g_strcmp0(id, *conference) == 0) {
            g_strfreev(conference_list);
            return IS_CONFERENCE;
        }
    }
    g_strfreev(conference_list);

    return IS_DESTROYED;

}

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

    g_object_unref(h->settings);

    g_free(h);
}

/*
 * Handle destroy event in the video windows.
 */
static void
video_window_deleted_cb(G_GNUC_UNUSED GtkWidget *widget,
                        G_GNUC_UNUSED gpointer data)
{
    if (dbus_has_video_camera_started())
        dbus_stop_video_camera();
}

/*
 * Handle resizing and moving event in the video windows.
 * This is usefull to store the previous behaviour and restore the user
 * preferences using gsettings.
 */
static gboolean
video_window_configure_cb(GtkWidget *widget,
                          GdkEventConfigure *event,
                          gpointer data)
{
    VideoHandle *handle = (VideoHandle *) data;

    gint pos_x, pos_y;

    gtk_window_get_position(GTK_WINDOW(widget), &pos_x, &pos_y);

    if (video_is_local(handle->id)) {
        g_settings_set_int(handle->settings, "window-video-local-width", event->width);
        g_settings_set_int(handle->settings, "window-video-local-height", event->height);
        g_settings_set_int(handle->settings, "window-video-local-position-x", pos_x);
        g_settings_set_int(handle->settings, "window-video-local-position-y", pos_y);
    } else {
        g_settings_set_int(handle->settings, "window-video-remote-width", event->width);
        g_settings_set_int(handle->settings, "window-video-remote-height", event->height);
        g_settings_set_int(handle->settings, "window-video-remote-position-x", pos_x);
        g_settings_set_int(handle->settings, "window-video-remote-position-y", pos_y);
    }

    /* let the event propagate otherwise the video will not be re-scaled */
    return FALSE;
}

/*
 * Handle button event in the video windows.
 */
static void
video_window_button_cb(GtkWindow *win,
                       GdkEventButton *event,
                       gpointer data)
{
    VideoHandle *handle = (VideoHandle *) data;

    if (event->type == GDK_2BUTTON_PRESS) {

        /* Fullscreen switch on/off */
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
    handle->settings = g_settings_new(SFLPHONE_GSETTINGS_SCHEMA);
    handle->fullscreen = FALSE;

    /* Get configuration stored in GSettings */
    gint width, height, pos_x, pos_y;
    if (video_is_local(id)) {
        width  = g_settings_get_int(handle->settings, "window-video-local-width");
        height = g_settings_get_int(handle->settings, "window-video-local-height");
        pos_x  = g_settings_get_int(handle->settings, "window-video-local-position-x");
        pos_y  = g_settings_get_int(handle->settings, "window-video-local-position-y");
    } else {
        width  = g_settings_get_int(handle->settings, "window-video-remote-width");
        height = g_settings_get_int(handle->settings, "window-video-remote-height");
        pos_x  = g_settings_get_int(handle->settings, "window-video-remote-position-x");
        pos_y  = g_settings_get_int(handle->settings, "window-video-remote-position-y");
    }

    /* Restore the previous setting for the video size and position */
    gtk_window_set_default_size(GTK_WINDOW(handle->window), width, height);
    gtk_window_move(GTK_WINDOW(handle->window), pos_x, pos_y);

    /* handle button event */
    g_signal_connect(handle->window, "button_press_event",
            G_CALLBACK(video_window_button_cb),
            handle);

    /* handle delete event */
    g_signal_connect(handle->window, "delete-event",
            G_CALLBACK(video_window_deleted_cb),
            NULL);

    /* handle configure event */
    g_signal_connect(handle->window, "configure-event",
            G_CALLBACK(video_window_configure_cb),
            handle);

    /* Preview video */
    if (video_is_local(id)) {

        gtk_window_set_title(GTK_WINDOW(handle->window), _("Local View"));
        update_camera_button_label();

    } else {

        gchar *window_title = NULL;
        gchar *title_prefix = NULL;
        gchar *name = NULL;
        call_type_t call_type = is_call_or_conf(id);

        if (call_type == IS_DESTROYED)
            return NULL;

        if (call_type == IS_CONFERENCE) { /* on a conference call */

            /* build the prefix title name */
            title_prefix = _("Conference with");

            /* get all the participants name */
            gchar **display_names = dbus_get_display_names(id);
            name = g_strjoinv(", ", display_names);
            g_strfreev(display_names);

        } else if (call_type == IS_CALL) { /* on a simple call */

            GHashTable *details = dbus_get_call_details(id);

            /* build the prefix title name */
            title_prefix = _("Call with");

            /* if no display name, we show the peer_number */
            const gchar *display_name = g_hash_table_lookup(details, "DISPLAY_NAME");
            if (strlen(display_name) != 0)
                name = g_strdup(display_name);
            else
                name = g_strdup(g_hash_table_lookup(details, "PEER_NUMBER"));
        }

        /* build the final title name */
        window_title = g_strjoin(" ", title_prefix, name, NULL);

        /* update the window title */
        gtk_window_set_title(GTK_WINDOW(handle->window), window_title);

        g_free(name);
        g_free(window_title);
    }

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

/* TODO: this window must be created at startup */
void
started_decoding_video_cb(G_GNUC_UNUSED DBusGProxy *proxy,
                          gchar *id,
                          gchar *shm_path,
                          gint width,
                          gint height,
                          G_GNUC_UNUSED gboolean is_mixer,
                          G_GNUC_UNUSED GError *error,
                          G_GNUC_UNUSED gpointer userdata)
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
                          G_GNUC_UNUSED gboolean is_mixer,
                          G_GNUC_UNUSED GError *error,
                          G_GNUC_UNUSED gpointer userdata)
{
    if (video_handles)
        g_hash_table_remove(video_handles, id);
}
