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

#ifndef __VIDEO_RENDERER_H__
#define __VIDEO_RENDERER_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#include "dbus.h"

G_BEGIN_DECLS

#define VIDEO_RENDERER_TYPE              (video_renderer_get_type())
#define VIDEO_RENDERER(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), VIDEO_RENDERER_TYPE, VideoRenderer))
#define VIDEO_RENDERER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), VIDEO_RENDERER_TYPE, VideoRendererClass))
#define IS_VIDEO_RENDERER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), VIDEO_RENDERER_TYPE))
#define IS_VIDEO_RENDERER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), VIDEO_RENDERER_TYPE))
#define VIDEO_RENDERER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), VIDEO_RENDERER_TYPE, VideoRendererClass))
#define VIDEO_RENDERER_CAST(obj)         ((VideoRenderer*)(obj))
#define VIDEO_RENDERER_CLASS_CAST(klass) ((VideoRendererClass*)(klass))

typedef struct _VideoRenderer      VideoRenderer;
typedef struct _VideoRendererClass VideoRendererClass;

typedef struct _VideoRendererPrivate VideoRendererPrivate;

struct _VideoRenderer {
    GObject parent;
    /* Private */
    VideoRendererPrivate *priv;
};

struct _VideoRendererClass {
    GObjectClass parent_class;
};

/* Public interface */
VideoRenderer *video_renderer_new(GtkWidget *drawarea, int width, int height, int shmkey, int semkey, int vbsize);
int video_renderer_run(VideoRenderer *preview);
void video_renderer_stop(VideoRenderer *preview);

void receiving_video_event_cb(DBusGProxy *proxy, gint shmId, gint semId,
                              gint videoBufferSize, gint destWidth,
                              gint destHeight, GError *error,
                              gpointer userdata);
void stopped_receiving_video_event_cb(DBusGProxy *proxy, gint shmId, gint semId, GError *error, gpointer userdata);

/* Try to init the gtk clutter backend, returns TRUE on success, FALSE otherwise */
gboolean try_clutter_init();

G_END_DECLS

#endif // __VIDEO_RENDERER_H__
