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

#ifndef __VIDEO_PREVIEW_H__
#define __VIDEO_PREVIEW_H__

#include <glib-object.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define VIDEO_PREVIEW_TYPE              (video_preview_get_type())
#define VIDEO_PREVIEW(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), VIDEO_PREVIEW_TYPE, VideoPreview))
#define VIDEO_PREVIEW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), VIDEO_PREVIEW_TYPE, VideoPreviewClass))
#define IS_VIDEO_PREVIEW(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), VIDEO_PREVIEW_TYPE))
#define IS_VIDEO_PREVIEW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), VIDEO_PREVIEW_TYPE))
#define VIDEO_PREVIEW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), VIDEO_PREVIEW_TYPE, VideoPreviewClass))
#define VIDEO_PREVIEW_CAST(obj)         ((VideoPreview*)(obj))
#define VIDEO_PREVIEW_CLASS_CAST(klass) ((VideoPreviewClass*)(klass))

typedef struct _VideoPreview      VideoPreview;
typedef struct _VideoPreviewClass VideoPreviewClass;

typedef struct _VideoPreviewPrivate VideoPreviewPrivate;

struct _VideoPreview {
    GObject parent;
    /* Private */
    VideoPreviewPrivate *priv;
};

struct _VideoPreviewClass {
    GObjectClass parent_class;
};

/* Public interface */
VideoPreview *video_preview_new(GtkWidget *drawarea, int width, int height, const char *format, int shmkey, int semkey, int vbsize);
void video_preview_run(VideoPreview *preview);
void video_preview_stop(VideoPreview *preview);

G_END_DECLS

#endif // __VIDEO_PREVIEW_H__
