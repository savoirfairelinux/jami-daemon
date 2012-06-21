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
#include "shm_header.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>

#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>

#include "actions.h"
#include "logger.h"
#include "unused.h"

static GtkWidget *video_window = NULL;
static gboolean video_window_fullscreen = FALSE;
static GtkWidget *video_area = NULL;
static VideoRenderer *video_renderer_global = NULL;

/* This macro will implement the video_renderer_get_type function
   and define a parent class pointer accessible from the whole .c file */
G_DEFINE_TYPE(VideoRenderer, video_renderer, G_TYPE_OBJECT);

#define VIDEO_RENDERER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
            VIDEO_RENDERER_TYPE, VideoRendererPrivate))

enum
{
    PROP_0,
    PROP_RUNNING,
    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_DRAWAREA,
    PROP_SHM_PATH,
    PROP_LAST
};

static GParamSpec *properties[PROP_LAST] = { NULL, };

static void video_renderer_finalize(GObject *gobject);
static void video_renderer_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);
static void video_renderer_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);

/* Our private member structure */
struct _VideoRendererPrivate {
    guint width;
    guint height;
    gchar *shm_path;

    ClutterActor *texture;

    gpointer drawarea;
    gboolean is_running;
    gint fd;
    SHMHeader *shm_area;
    gsize shm_area_len;
    guint buffer_gen;
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

    properties[PROP_WIDTH] = g_param_spec_int("width", "Width", "Width of video", G_MININT, G_MAXINT, 0,
                                              G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY);

    properties[PROP_HEIGHT] = g_param_spec_int("height", "Height", "Height of video", G_MININT, G_MAXINT, 0,
                                               G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY);

    properties[PROP_SHM_PATH] = g_param_spec_string("shm-path", "ShmPath", "Unique path for shared memory", "",
                                                    G_PARAM_READABLE|G_PARAM_WRITABLE|G_PARAM_CONSTRUCT_ONLY);

    g_object_class_install_properties(gobject_class,
            PROP_LAST,
            properties);
}

static void
video_renderer_get_property(GObject *object, guint prop_id,
                            GValue *value, GParamSpec *pspec)
{
    VideoRenderer *renderer = VIDEO_RENDERER(object);
    VideoRendererPrivate *priv = renderer->priv;

    switch (prop_id) {
        case PROP_RUNNING:
            g_value_set_boolean(value, priv->is_running);
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

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}


static void
video_renderer_init(VideoRenderer *self)
{
    VideoRendererPrivate *priv;
    self->priv = priv = VIDEO_RENDERER_GET_PRIVATE(self);
    priv->width = 0;
    priv->height = 0;
    priv->shm_path = NULL;
    priv->texture = NULL;
    priv->drawarea = NULL;
    priv->is_running = FALSE;
    priv->fd = -1;
    priv->shm_area = MAP_FAILED;
    priv->shm_area_len = 0;
    priv->buffer_gen = 0;
}

static void
video_renderer_stop_shm(VideoRenderer *self)
{
    g_return_if_fail(IS_VIDEO_RENDERER(self));
    VideoRendererPrivate *priv = VIDEO_RENDERER_GET_PRIVATE(self);
    if (priv->fd >= 0)
        close(priv->fd);
    priv->fd = -1;

    if (priv->shm_area != MAP_FAILED)
        munmap(priv->shm_area, priv->shm_area_len);
    priv->shm_area_len = 0;
    priv->shm_area = MAP_FAILED;
}


static void
video_renderer_finalize(GObject *obj)
{
    DEBUG("finalize");
    VideoRenderer *self = VIDEO_RENDERER(obj);
    video_renderer_stop_shm(self);

    /* Chain up to the parent class */
    G_OBJECT_CLASS(video_renderer_parent_class)->finalize(obj);
}

static gboolean
video_renderer_start_shm(VideoRenderer *self)
{
    /* First test that 'self' is of the correct type */
    g_return_val_if_fail(IS_VIDEO_RENDERER(self), FALSE);
    VideoRendererPrivate *priv = VIDEO_RENDERER_GET_PRIVATE(self);
    if (priv->fd != -1) {
        ERROR("fd must be -1");
        return FALSE;
    }

    priv->fd = shm_open(priv->shm_path, O_RDWR, 0);
    if (priv->fd < 0) {
        DEBUG("could not open shm area \"%s\", shm_open failed:%s", priv->shm_path, strerror(errno));
        return FALSE;
    }
    priv->shm_area_len = sizeof(SHMHeader);
    priv->shm_area = mmap(NULL, priv->shm_area_len, PROT_READ | PROT_WRITE, MAP_SHARED, priv->fd, 0);
    if (priv->shm_area == MAP_FAILED) {
        DEBUG("Could not map shm area, mmap failed");
        return FALSE;
    }
    return TRUE;
}

static void
shm_lock(SHMHeader *shm_area)
{
    sem_wait(&shm_area->mutex);
}

static void
shm_unlock(SHMHeader *shm_area)
{
    sem_post(&shm_area->mutex);
}

static gboolean
video_renderer_resize_shm(VideoRendererPrivate *priv)
{
    while ((sizeof(SHMHeader) + priv->shm_area->buffer_size) > priv->shm_area_len) {
        const size_t new_size = sizeof(SHMHeader) + priv->shm_area->buffer_size;

        shm_unlock(priv->shm_area);
        if (munmap(priv->shm_area, priv->shm_area_len)) {
            DEBUG("Could not unmap shared area:%s", strerror(errno));
            return FALSE;
        }

        priv->shm_area = mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, priv->fd, 0);
        priv->shm_area_len = new_size;

        if (!priv->shm_area) {
            priv->shm_area = 0;
            DEBUG("Could not remap shared area");
            return FALSE;
        }

        priv->shm_area_len = new_size;
        shm_lock(priv->shm_area);
    }
    return TRUE;
}

static void
video_renderer_render_to_texture(VideoRendererPrivate *priv)
{
    shm_lock(priv->shm_area);

    while (priv->buffer_gen == priv->shm_area->buffer_gen) {
        shm_unlock(priv->shm_area);
        sem_wait(&priv->shm_area->notification);
        shm_lock(priv->shm_area);
    }

    if (!video_renderer_resize_shm(priv)) {
        ERROR("Could not resize shared memory");
        return;
    }

    const gint BPP = 4;
    const gint ROW_STRIDE = BPP * priv->width;
    /* update the clutter texture */
    clutter_texture_set_from_rgb_data(CLUTTER_TEXTURE(priv->texture),
            (guchar*) priv->shm_area->data,
            TRUE,
            priv->width,
            priv->height,
            ROW_STRIDE,
            BPP,
            CLUTTER_TEXTURE_RGB_FLAG_BGR,
            NULL);
    priv->buffer_gen = priv->shm_area->buffer_gen;
    shm_unlock(priv->shm_area);
}


static gboolean
render_frame_from_shm(VideoRendererPrivate *priv)
{
    GtkWidget *parent = gtk_widget_get_parent(priv->drawarea);
    const gint parent_width = gtk_widget_get_allocated_width(parent);
    const gint parent_height = gtk_widget_get_allocated_height(parent);

    clutter_actor_set_size(priv->texture, parent_width, parent_height);
    video_renderer_render_to_texture(priv);

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
video_renderer_new(GtkWidget *drawarea, gint width, gint height, gchar *shm_path)
{
    VideoRenderer *rend = g_object_new(VIDEO_RENDERER_TYPE, "drawarea", (gpointer) drawarea,
            "width", width, "height", height, "shm-path", shm_path, NULL);
    if (!video_renderer_start_shm(rend)) {
        ERROR("Could not start SHM");
        return NULL;
    }
    return rend;
}

gboolean
video_renderer_run(VideoRenderer *self)
{
    g_return_val_if_fail(IS_VIDEO_RENDERER(self), FALSE);
    VideoRendererPrivate * priv = VIDEO_RENDERER_GET_PRIVATE(self);
    g_return_val_if_fail(priv->fd > 0, FALSE);

    GtkWindow *win = GTK_WINDOW(gtk_widget_get_toplevel(priv->drawarea));
    GdkGeometry geom = {
        .min_aspect = (gdouble) priv->width / priv->height,
        .max_aspect = (gdouble) priv->width / priv->height,
    };
    gtk_window_set_geometry_hints(win, NULL, &geom, GDK_HINT_ASPECT);

    if (GTK_CLUTTER_IS_EMBED(priv->drawarea))
        DEBUG("using clutter\n");
    else
        ERROR("Drawing area is not a GtkClutterEmbed widget");

    ClutterActor *stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(priv->drawarea));
    g_assert(stage);
    priv->texture = clutter_texture_new();

    /* Add ClutterTexture to the stage */
    clutter_container_add(CLUTTER_CONTAINER(stage), priv->texture, NULL);
    clutter_actor_show_all(stage);

    /* frames are read and saved here */
    g_object_ref(self);
    const gdouble FPS = 30.0;
    g_timeout_add(1000 / FPS, update_texture, self);

    priv->is_running = TRUE;
    /* emit the notify signal for this property */
#if GLIB_CHECK_VERSION(2, 26, 0)
    g_object_notify_by_pspec(G_OBJECT(self), properties[PROP_RUNNING]);
#else
    g_object_notify(G_OBJECT(data), "running");
#endif
    gtk_widget_show_all(GTK_WIDGET(priv->drawarea));

    return TRUE;
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

void started_decoding_video_cb(DBusGProxy *proxy UNUSED,
        gchar *id, gchar *shm_path, gint width, gint height,
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

    if (shm_path == 0 || strlen(shm_path) == 0)
        return;

    gtk_widget_set_size_request(video_area, width, height);
    gtk_container_add(GTK_CONTAINER(video_window), vbox);
    gtk_widget_show_all(video_window);

    DEBUG("Video started for shm:%s width:%d height:%d",
           shm_path, width, height);

    video_renderer_global = video_renderer_new(video_area, width, height, shm_path);
    g_assert(video_renderer_global);
    if (!video_renderer_run(video_renderer_global)) {
        g_object_unref(video_renderer_global);
        video_renderer_global = NULL;
        ERROR("Could not run video renderer");
    }
}

void
stopped_decoding_video_cb(DBusGProxy *proxy UNUSED, gchar *id, gchar *shm_path, GError *error UNUSED, gpointer userdata UNUSED)
{
    DEBUG("Video stopped for id %s, shm path %s", id, shm_path);

    if (video_renderer_global) {
        if (video_window) {
            if (GTK_IS_WIDGET(video_window))
                gtk_widget_destroy(video_window);
            video_area = video_window = NULL;
        }
        video_renderer_global = NULL;
    }
}
