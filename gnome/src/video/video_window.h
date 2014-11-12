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

#ifndef __VIDEO_WINDOW_H__
#define __VIDEO_WINDOW_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#define VIDEO_WINDOW_TYPE              (video_window_get_type())
#define VIDEO_WINDOW(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), VIDEO_WINDOW_TYPE, VideoWindow))
#define VIDEO_WINDOW_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), VIDEO_WINDOW_TYPE, VideoWindowClass))
#define IS_VIDEO_WINDOW(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), VIDEO_WINDOW_TYPE))
#define IS_VIDEO_WINDOW_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), VIDEO_WINDOW_TYPE))
#define VIDEO_WINDOW_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), VIDEO_WINDOW_TYPE, VideoWindowClass))

typedef struct _VideoWindowPrivate VideoWindowPrivate;
typedef struct _VideoWindowClass VideoWindowClass;
typedef struct _VideoWindow VideoWindow;

struct _VideoWindowClass {
    GtkWindowClass parent_class;
};

struct _VideoWindow {
    GtkWindow parent;
    /* Private */
    VideoWindowPrivate *priv;
};

/* Public interface */
GType           video_window_get_type           (void) G_GNUC_CONST;
GtkWidget*      video_window_new                (void);

#endif // __VIDEO_WINDOW_H__