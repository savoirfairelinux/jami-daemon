/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include "video_renderer.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>

#include "actions.h"
#include "logger.h"
#include "unused.h"

static GtkWidget *video_window = NULL;
static gboolean video_window_fullscreen = FALSE;
static GtkWidget *video_area = NULL;
static VideoRenderer *video_renderer = NULL;

#define VIDEO_RENDERER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
            VIDEO_RENDERER_TYPE, VideoRendererPrivate))

/* This macro will implement the video_renderer_get_type function
   and define a parent class pointer accessible from the whole .c file */
G_DEFINE_TYPE(VideoRenderer, video_renderer, G_TYPE_OBJECT);

enum
{
    PROP_0,
    PROP_RUNNING,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_DRAWAREA,
    PROP_SHM_PATH,
    PROP_VIDEO_BUFFER_SIZE,
    PROP_LAST
};

static GParamSpec *properties[PROP_LAST];

static void video_renderer_finalize(GObject *gobject);
static void video_renderer_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void video_renderer_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

/* Our private member structure */
struct _VideoRendererPrivate {
    guint width;
    guint height;
    gint videobuffersize;
    gchar *shm_path;

    ClutterActor *texture;

    gpointer drawarea;
    gboolean is_running;
};

static void
video_renderer_class_init(VideoRendererClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(VideoRendererPrivate));
    gobject_class->finalize = video_renderer_finalize;
    gobject_class->get_property = video_renderer_get_property;
    gobject_class->set_property = video_renderer_set_property;

    properties[PROP_RUNNING] = g_param_spec_boolean("running", "Running",
                                                    "True if renderer is running",
                                                    FALSE,
                                                    G_PARAM_READABLE);

    properties[PROP_DRAWAREA] = g_param_spec_pointer("drawarea", "DrawArea",
                                                    "Pointer to the drawing area",
                                                    G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY);

    properties[PROP_WIDTH] = g_param_spec_int("width", "Width", "Width of video", G_MININT, G_MAXINT, -1,
                                              G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY);

    properties[PROP_HEIGHT] = g_param_spec_int("height", "Height", "Height of video", G_MININT, G_MAXINT, -1,
                                               G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY);

    properties[PROP_SHM_PATH] = g_param_spec_string("shm-path", "ShmPath", "Unique path for shared memory", "",
                                                    G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY);

    properties[PROP_VIDEO_BUFFER_SIZE] = g_param_spec_int("vbsize", "VideoBufferSize", "Size of shared memory buffer", G_MININT, G_MAXINT, -1,
                                                          G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY);

    for (gint i = PROP_0 + 1; i < PROP_LAST; ++i)
        g_object_class_install_property(gobject_class, i, properties[i]);
}

static void
video_renderer_get_property(GObject *object, guint prop_id,
                            GValue *value, GParamSpec *pspec)
{
    VideoRenderer *renderer = VIDEO_RENDERER(object);
    VideoRendererPrivate *priv = renderer->priv;

    switch (prop_id) {
        case PROP_RUNNING:
            g_value_set_boolean (value, priv->is_running);
            break;
        case PROP_DRAWAREA:
            g_value_set_pointer(value, priv->drawarea);
            break;
        case PROP_WIDTH:
            g_value_set_int(value, priv->width);
            break;
        case PROP_HEIGHT:
            g_value_set_int(value, priv->height);
            break;
        case PROP_SHM_PATH:
            g_value_set_string(value, priv->shm_path);
            break;
        case PROP_VIDEO_BUFFER_SIZE:
            g_value_set_int(value, priv->videobuffersize);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
video_renderer_set_property (GObject *object, guint prop_id,
                            const GValue *value, GParamSpec *pspec)
{
    VideoRenderer *renderer = VIDEO_RENDERER(object);
    VideoRendererPrivate *priv = renderer->priv;

    switch (prop_id) {
        case PROP_DRAWAREA:
            priv->drawarea = g_value_get_pointer(value);
            break;
        case PROP_WIDTH:
            priv->width = g_value_get_int(value);
            break;
        case PROP_HEIGHT:
            priv->height = g_value_get_int(value);
            break;
        case PROP_SHM_PATH:
            g_free(priv->shm_path);
            priv->shm_path = g_value_dup_string(value);
            break;
        case PROP_VIDEO_BUFFER_SIZE:
            priv->videobuffersize = g_value_get_int(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}


static void
video_renderer_init (VideoRenderer *self)
{
    self->priv = VIDEO_RENDERER_GET_PRIVATE(self);
}

static void
video_renderer_finalize (GObject *obj)
{
    /*VideoRenderer *self = VIDEO_RENDERER (obj);*/

    /* Chain up to the parent class */
    G_OBJECT_CLASS (video_renderer_parent_class)->finalize (obj);
}

static void
render_clutter(ClutterActor *texture, gpointer data, gint width,
               gint height, gint parent_width, gint parent_height)
{
    clutter_actor_set_size(texture, parent_width, parent_height);

    const gint ROW_STRIDE = 4 * width;
    const gint BPP = 4;
    clutter_texture_set_from_rgb_data(CLUTTER_TEXTURE(texture), data, TRUE,
                                      width, height, ROW_STRIDE, BPP,
                                      CLUTTER_TEXTURE_RGB_FLAG_BGR, NULL);
}

static gboolean
render_frame_from_shm(VideoRendererPrivate *priv)
{
    GtkWidget *parent = gtk_widget_get_parent(priv->drawarea);
    const gint p_width = gtk_widget_get_allocated_width(parent);
    const gint p_height = gtk_widget_get_allocated_height(parent);

    // FIXME:
    char *data = NULL;
    render_clutter(priv->texture, data, priv->width,
                   priv->height, p_width, p_height);

    return TRUE;
}

void video_renderer_stop(VideoRenderer *renderer)
{
    VideoRendererPrivate *priv = VIDEO_RENDERER_GET_PRIVATE(renderer);
    g_assert(priv);
    gtk_widget_hide(GTK_WIDGET(priv->drawarea));

    priv->is_running = FALSE;

#if GLIB_CHECK_VERSION(2,26,0)
    g_object_notify_by_pspec(G_OBJECT(renderer), properties[PROP_RUNNING]);
#else
    g_object_notify(G_OBJECT(renderer), "running");
#endif

    g_object_unref(G_OBJECT(renderer));
}

static gboolean
update_texture(gpointer data)
{
    VideoRenderer *renderer = (VideoRenderer *) data;
    VideoRendererPrivate *priv = VIDEO_RENDERER_GET_PRIVATE(renderer);

    if (!priv->is_running) {
        /* renderer was stopped already */
        g_object_unref(G_OBJECT(data));
        return FALSE;
    }

    const gboolean ret = render_frame_from_shm(priv);

    if (!ret) {
        priv->is_running = FALSE; // no need to notify daemon
        video_renderer_stop(data);
        g_object_unref(G_OBJECT(data));
    }

    return ret;
}

/**
 * video_renderer_new:
 *
 * Create a new #VideoRenderer instance.
 */
VideoRenderer *
video_renderer_new(GtkWidget *drawarea, gint width, gint height, gchar *shm_path, gint vbsize)
{
    return g_object_new(VIDEO_RENDERER_TYPE, "drawarea", (gpointer) drawarea,
                        "width", width, "height", height, "shm-path", shm_path,
                        "vbsize", vbsize, NULL);
}

int
video_renderer_run(VideoRenderer *renderer)
{
    VideoRendererPrivate * priv = VIDEO_RENDERER_GET_PRIVATE(renderer);

    GtkWindow *win = GTK_WINDOW(gtk_widget_get_toplevel(priv->drawarea));
    GdkGeometry geom = {
        .min_aspect = (double) priv->width / priv->height,
        .max_aspect = (double) priv->width / priv->height,
    };
    gtk_window_set_geometry_hints(win, NULL, &geom, GDK_HINT_ASPECT);

    if (GTK_CLUTTER_IS_EMBED(priv->drawarea))
        DEBUG("video_renderer: using clutter\n");
    else
        ERROR("Drawing area is not a GtkClutterEmbed widget");

    ClutterActor *stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(priv->drawarea));
    g_assert(stage);
    priv->texture = clutter_texture_new();

    /* Add ClutterTexture to the stage */
    clutter_container_add(CLUTTER_CONTAINER(stage), priv->texture, NULL);
    clutter_actor_show_all(stage);

    /* frames are read and saved here */
    g_object_ref(renderer);
    const gdouble FPS = 30.0;
    g_timeout_add(1000 / FPS, update_texture, renderer);

    priv->is_running = TRUE;
    /* emit the notify signal for this property */
#if GLIB_CHECK_VERSION(2, 26, 0)
    g_object_notify_by_pspec(G_OBJECT(renderer), properties[PROP_RUNNING]);
#else
    g_object_notify(G_OBJECT(data), "running");
#endif
    gtk_widget_show_all(GTK_WIDGET(priv->drawarea));

    return 0;
}

static void
video_window_deleted_cb(GtkWidget *widget UNUSED,
                                  gpointer data UNUSED)
{
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

gboolean
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

void started_video_event_cb(DBusGProxy *proxy UNUSED, gchar *shm_path,
                            gint buffer_size, gint width, gint height,
                            GError *error UNUSED, gpointer userdata UNUSED)
{
    if (!video_window) {
        video_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
        video_window_fullscreen = FALSE;
        g_signal_connect(video_window, "button_press_event",
                         G_CALLBACK(video_window_button_cb),
                         &video_window_fullscreen);
        g_signal_connect(video_window, "delete-event",
                         G_CALLBACK(video_window_deleted_cb), NULL);
    }

    if (!try_clutter_init())
        return;

    if (!video_area) {
        video_area = gtk_clutter_embed_new();
        ClutterActor *stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(video_area));
        if (!stage)
            gtk_widget_destroy(video_area);
        else {
            ClutterColor stage_color = { 0x61, 0x64, 0x8c, 0xff };
            clutter_stage_set_color(CLUTTER_STAGE(stage), &stage_color);
        }
    }

    g_assert(video_area);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(vbox), video_area);

    if (shm_path == 0 || strlen(shm_path) == 0 || buffer_size <= 0)
        return;

    gtk_widget_set_size_request(video_area, width, height);
    gtk_container_add(GTK_CONTAINER(video_window), vbox);
    gtk_widget_show_all(video_window);

    DEBUG("Video started for shm:%s buffer_sz:%d width:%d height:%d",
           shm_path, buffer_size, width, height);

    video_renderer = video_renderer_new(video_area, width, height, shm_path, buffer_size);
    g_assert(video_renderer);
    if (video_renderer_run(video_renderer)) {
        g_object_unref(video_renderer);
        video_renderer = NULL;
        ERROR("Could not run video renderer");
    }
}

void
stopped_video_event_cb(DBusGProxy *proxy UNUSED, gchar *shm_path, GError *error UNUSED, gpointer userdata UNUSED)
{
    DEBUG("Video stopped for shm:%s", shm_path);

    if (video_renderer) {
        if (video_window) {
            if (GTK_IS_WIDGET(video_window))
                gtk_widget_destroy(video_window);
            video_area = video_window = NULL;
        }
        video_renderer = NULL;
    }
}
