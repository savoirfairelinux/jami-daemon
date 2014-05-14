/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Sebastien Bourdelin <sebastien.bourdelin@savoirfairelinux.com>
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

#ifndef VIDEO_WIDGET_H__
#define VIDEO_WIDGET_H__

#include <glib-object.h>
#include <gtk/gtk.h>

#define VIDEO_WIDGET_TYPE              (video_widget_get_type())
#define VIDEO_WIDGET(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), VIDEO_WIDGET_TYPE, VideoWidget))
#define VIDEO_WIDGET_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), VIDEO_WIDGET_TYPE, VideoWidgetClass))
#define IS_VIDEO_WIDGET(obj)           (G_TYPE_CHECK_INSTANCE_TYPE((obj), VIDEO_WIDGET_TYPE))
#define IS_VIDEO_WIDGET_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE((klass), VIDEO_WIDGET_TYPE))

typedef struct _VideoWidgetPrivate VideoWidgetPrivate;
typedef struct _VideoWidgetClass VideoWidgetClass;
typedef struct _VideoWidget VideoWidget;

typedef enum {
    VIDEO_AREA_REMOTE,
    VIDEO_AREA_LOCAL,
    VIDEO_AREA_LAST
} VIDEO_AREA_ID;

struct _VideoWidgetClass {
    GtkWindowClass parent_class;
};

struct _VideoWidget {
    GtkWindow parent;
    /* Private */
    VideoWidgetPrivate *priv;
};

/* Public interface */
GType video_widget_get_type(void) G_GNUC_CONST;
GtkWidget *video_widget_new();
void video_widget_camera_start(GtkWidget *, VIDEO_AREA_ID, gchar *, gchar *, guint, guint, gboolean);
void video_widget_camera_stop(GtkWidget *self, VIDEO_AREA_ID, gchar *);

#endif // __VIDEO_WIDGET_H__
