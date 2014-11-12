/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Stepan Salenikovich <stepan.salenikovich@savoirfairelinux.com>
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

#include "video_window.h"

#include "sflphone_client.h"    /* gsettings schema path */

struct _VideoWindowPrivate {
    GtkWidget        *video;
    GtkWidget        *toolbar;
    GSettings        *settings;
};

/* Define the VideoWindow type and inherit from GtkWindow */
G_DEFINE_TYPE(VideoWindow, video_window, GTK_TYPE_WINDOW);

#define VIDEO_WINDOW_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
            VIDEO_WINDOW_TYPE, VideoWindowPrivate))

/* static prototypes */
static void     on_show_event_cb      (GtkWidget *, gpointer);
static gboolean on_configure_event_cb (GtkWidget *, GdkEventConfigure *, gpointer);


/*
 * video_window_finalize()
 *
 * The finalize function for the video_window class.
 */
static void
video_window_finalize(GObject *object)
{
    VideoWindow *self = VIDEO_WINDOW(object);
    VideoWindowPrivate *priv = VIDEO_WINDOW_GET_PRIVATE(self);

    /* unref gsettings object */
    g_object_unref(priv->settings);

    G_OBJECT_CLASS (video_window_parent_class)->finalize(object);
}


/*
 * video_window_class_init()
 *
 * This function init the video_window_class.
 */
static void
video_window_class_init(VideoWindowClass *klass)
{
    /* get the parent gobject class */
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    /* add private structure */
    g_type_class_add_private(klass, sizeof(VideoWindowPrivate));

    /* override method */
    object_class->finalize = video_window_finalize;
}


/*
 * video_window_init()
 *
 * This function init the video_window.
 * - init all the widget members
 */
static void
video_window_init(VideoWindow *self)
{
    VideoWindowPrivate *priv = self->priv = VIDEO_WINDOW_GET_PRIVATE(self);

    /* init widget */
    priv->video = NULL;
    priv->toolbar = NULL;
    priv->settings = g_settings_new(SFLPHONE_GSETTINGS_SCHEMA);
    // priv->fullscreen = FALSE;
}


/*
 * video_window_new()
 *
 * The function use to create a new video_window
 */
GtkWidget*
video_window_new(void)
{
    GtkWidget *self = g_object_new(VIDEO_WINDOW_TYPE, NULL);

    /* we don't want the window to be destroyed until the program ends */
    g_signal_connect(self, "delete-event",
                     G_CALLBACK(gtk_widget_hide_on_delete), NULL);

    g_signal_connect(self, "configure-event",
            G_CALLBACK(on_configure_event_cb),
            NULL);

    g_signal_connect(self, "show",
            G_CALLBACK(on_show_event_cb),
            NULL);

    return self;
}


/*
 * on_configure_event_cb()
 *
 * Save the current size and position of the window.
 * This is usefull to store the previous behaviour and restore the user
 * preferences using gsettings.
 */
static gboolean
on_configure_event_cb(GtkWidget *self,
    G_GNUC_UNUSED GdkEventConfigure *event,
                 G_GNUC_UNUSED gpointer data)
{
    g_debug("save position/size of video window");
    g_return_val_if_fail(IS_VIDEO_WINDOW(self), FALSE);

    /* ignore the position and size after the window is hidden */
    if (gtk_widget_is_visible(self)){
        VideoWindowPrivate *priv = VIDEO_WINDOW_GET_PRIVATE(self);

        /* get and store window size and position */
        gint pos_x, pos_y, width, height;
        gtk_window_get_size(GTK_WINDOW(self), &width, &height);
        gtk_window_get_position(GTK_WINDOW(self), &pos_x, &pos_y);
        g_settings_set_int(priv->settings, "video-widget-width",      width);
        g_settings_set_int(priv->settings, "video-widget-height",     height);
        g_settings_set_int(priv->settings, "video-widget-position-x", pos_x);
        g_settings_set_int(priv->settings, "video-widget-position-y", pos_y);
    }

    /* let the event propagate otherwise the video will not be re-scaled */
    return FALSE;
}


/*
 * on_show_event_cb()
 *
 * Restore the saved size and position of the window when it is shown.
 */
static void
on_show_event_cb(GtkWidget *self,
                 G_GNUC_UNUSED gpointer data)
{
    g_debug("restore position/size of video window");
    g_return_val_if_fail(IS_VIDEO_WINDOW(self), FALSE);

    VideoWindowPrivate *priv = VIDEO_WINDOW_GET_PRIVATE(self);

    /* retrieve the previous windows settings */
    gint width = 0, height = 0, pos_x = 0, pos_y = 0;
    pos_x  = g_settings_get_int(priv->settings, "video-widget-position-x");
    pos_y  = g_settings_get_int(priv->settings, "video-widget-position-y");
    width  = g_settings_get_int(priv->settings, "video-widget-width");
    height = g_settings_get_int(priv->settings, "video-widget-height");

    /* place  and resize the window according the users preferences */
    gtk_window_move(GTK_WINDOW(self), pos_x, pos_y);
    gtk_window_resize(GTK_WINDOW(self), width, height);
}
