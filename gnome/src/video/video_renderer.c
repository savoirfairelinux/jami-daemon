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

#include "video_renderer.h"
#include "shm_header.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>

#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>

/* This macro will implement the video_renderer_get_type function
   and define a parent class pointer accessible from the whole .c file */
G_DEFINE_TYPE(VideoRenderer, video_renderer, G_TYPE_OBJECT);

#define VIDEO_RENDERER_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
            VIDEO_RENDERER_TYPE, VideoRendererPrivate))

enum
{
    PROP_0,
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

    gint fd;
    SHMHeader *shm_area;
    gsize shm_area_len;
    guint buffer_gen;
    guint timeout_id;
};

static void
video_renderer_class_init(VideoRendererClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    g_type_class_add_private(klass, sizeof(VideoRendererPrivate));
    gobject_class->finalize = video_renderer_finalize;
    gobject_class->get_property = video_renderer_get_property;
    gobject_class->set_property = video_renderer_set_property;

    properties[PROP_DRAWAREA] = g_param_spec_pointer("texture", "Texture",
                                                    "Pointer to the Clutter Texture area",
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
        case PROP_DRAWAREA:
            g_value_set_pointer(value, priv->texture);
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
            priv->texture = g_value_get_pointer(value);
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
    priv->fd = -1;
    priv->shm_area = MAP_FAILED;
    priv->shm_area_len = 0;
    priv->buffer_gen = 0;
    priv->timeout_id = 0;
}

static void
video_renderer_stop_shm(VideoRenderer *self)
{
    g_return_if_fail(IS_VIDEO_RENDERER(self));
    VideoRendererPrivate *priv = VIDEO_RENDERER_GET_PRIVATE(self);

    g_source_remove(priv->timeout_id);

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
        g_warning("fd must be -1");
        return FALSE;
    }

    priv->fd = shm_open(priv->shm_path, O_RDWR, 0);
    if (priv->fd < 0) {
        g_debug("could not open shm area \"%s\", shm_open failed:%s", priv->shm_path, strerror(errno));
        return FALSE;
    }
    priv->shm_area_len = sizeof(SHMHeader);
    priv->shm_area = mmap(NULL, priv->shm_area_len, PROT_READ | PROT_WRITE, MAP_SHARED, priv->fd, 0);
    if (priv->shm_area == MAP_FAILED) {
        g_debug("Could not map shm area, mmap failed");
        return FALSE;
    }
    return TRUE;
}

static gboolean
shm_lock(SHMHeader *shm_area)
{
    int err = sem_trywait(&shm_area->mutex);
    if (err < 0 && errno != EAGAIN)
        g_warning("Renderer: sem_trywait() failed, %s", strerror(errno));
    return !err;
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
            g_debug("Could not unmap shared area:%s", strerror(errno));
            return FALSE;
        }

        priv->shm_area = mmap(NULL, new_size, PROT_READ | PROT_WRITE, MAP_SHARED, priv->fd, 0);
        priv->shm_area_len = new_size;

        if (!priv->shm_area) {
            priv->shm_area = 0;
            g_debug("Could not remap shared area");
            return FALSE;
        }

        priv->shm_area_len = new_size;
        if (!shm_lock(priv->shm_area))
            return FALSE;
    }
    return TRUE;
}

static void
video_renderer_render_to_texture(VideoRendererPrivate *priv)
{

    if (!shm_lock(priv->shm_area))
        return;

    // Just return if nothing ready yet
    if (priv->buffer_gen == priv->shm_area->buffer_gen) {
        shm_unlock(priv->shm_area);
        return;
    }

    if (!video_renderer_resize_shm(priv)) {
        g_warning("Could not resize shared memory");
        return;
    }

    const gint BPP = 4;
    const gint ROW_STRIDE = BPP * priv->width;
    const gint bytes_to_read = ROW_STRIDE * priv->height;
    if (bytes_to_read <= priv->shm_area->buffer_size) {
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
    } else {
        g_warning("Trying to read %d bytes from shared buffer of size %d bytes",
                  bytes_to_read, priv->shm_area->buffer_size);
    }
    priv->buffer_gen = priv->shm_area->buffer_gen;
    shm_unlock(priv->shm_area);
}


static gboolean
render_frame_from_shm(VideoRendererPrivate *priv)
{
    if (!priv || !CLUTTER_IS_ACTOR(priv->texture))
        return FALSE;

    video_renderer_render_to_texture(priv);

    return TRUE;
}

void video_renderer_stop(VideoRenderer *renderer)
{
    g_object_unref(G_OBJECT(renderer));
}

static gboolean
update_texture(gpointer data)
{
    VideoRenderer *renderer = (VideoRenderer *) data;
    VideoRendererPrivate *priv = VIDEO_RENDERER_GET_PRIVATE(renderer);

    const gboolean ret = render_frame_from_shm(priv);

    if (!ret)
        video_renderer_stop(data);

    return ret;
}

/**
 * video_renderer_new:
 *
 * Create a new #VideoRenderer instance.
 */
VideoRenderer *
video_renderer_new(ClutterActor *texture, gint width, gint height, gchar *shm_path)
{
    VideoRenderer *rend = g_object_new(VIDEO_RENDERER_TYPE, "texture", texture,
            "width", width, "height", height, "shm-path", shm_path, NULL);
    if (!video_renderer_start_shm(rend)) {
        g_warning("Could not start SHM");
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

    /* frames are read and saved here */
    const gint FRAME_INTERVAL = 30; // ms
    priv->timeout_id = g_timeout_add_full(G_PRIORITY_DEFAULT_IDLE, FRAME_INTERVAL, update_texture, self, NULL);

    return TRUE;
}
