/*
 *  Copyright (C) 2012 Savoir-Faire Linux Inc.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  The Rhythmbox authors hereby grant permission for non-GPL compatible
 *  GStreamer plugins to be used and distributed together with GStreamer
 *  and Rhythmbox. This permission is above and beyond the permissions granted
 *  by the GPL license by which Rhythmbox is covered. If you modify this code
 *  you may extend this exception to your version of the code, but you are not
 *  obligated to do so. If you do not wish to do so, delete this exception
 *  statement from your version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA.
 *
 */

#ifndef __SFL_SEEKSLIDER_H
#define __SFL_SEEKSLIDER_H

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define SFL_TYPE_SEEKSLIDER         (sfl_seekslider_get_type ())
#define SFL_SEEKSLIDER(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), SFL_TYPE_SEEKSLIDER, SFLSeekSlider))
#define SFL_SEEKSLIDER_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST((k), SFL_TYPE_SEEKSLIDER, SFLSeekSliderClass))
#define SFL_IS_SEEKSLIDER(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), SFL_TYPE_SEEKSLIDER))
#define SFL_IS_SEEKSLIDER_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), SFL_TYPE_SEEKSLIDER))
#define SFL_SEEKSLIDER_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), SFL_TYPE_SEEKSLIDER, SFLSeekSliderClass))

typedef struct _SFLSeekSlider SFLSeekSlider;
typedef struct _SFLSeekSliderClass SFLSeekSliderClass;

typedef struct SFLSeekSliderPrivate SFLSeekSliderPrivate;

struct _SFLSeekSlider
{
    GtkHBox parent;
    SFLSeekSliderPrivate *priv;
};

struct _SFLSeekSliderClass
{
    GtkHBoxClass parent;
};

typedef enum {
    SFL_SEEKSLIDER_DISPLAY_PAUSE,
    SFL_SEEKSLIDER_DISPLAY_PLAY,
} SFLSeekSliderDisplay;

GType sfl_seekslider_get_type(void);

SFLSeekSlider *sfl_seekslider_new(void);

void sfl_seekslider_update_scale(SFLSeekSlider *seekslider, guint current, guint size);

void sfl_seekslider_set_display(SFLSeekSlider *seekslider, SFLSeekSliderDisplay display);

G_END_DECLS

#endif /* __RB_SEEKSLIDER_H */
