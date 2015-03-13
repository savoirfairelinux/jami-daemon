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

#ifndef __VIDEO_ASPECT_FRAME_H__
#define __VIDEO_ASPECT_FRAME_H__

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define VIDEO_ASPECT_FRAME_TYPE			video_aspect_frame_get_type()
#define VIDEO_ASPECT_FRAME(obj)			(G_TYPE_CHECK_INSTANCE_CAST((obj), VIDEO_ASPECT_FRAME_TYPE, VideoAspectFrame))
#define VIDEO_ASPECT_FRAME_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST((klass),  VIDEO_ASPECT_FRAME_TYPE, VideoAspectFrameClass))
#define IS_VIDEO_ASPECT_FRAME(obj)		(G_TYPE_CHECK_INSTANCE_TYPE((obj), VIDEO_ASPECT_FRAME_TYPE))
#define IS_VIDEO_ASPECT_FRAME_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE((klass),  VIDEO_ASPECT_FRAME_TYPE))

typedef struct _VideoAspectFrameClass VideoAspectFrameClass;
typedef struct _VideoAspectFrame VideoAspectFrame;

struct _VideoAspectFrameClass
{
    ClutterActorClass parent_class;
};

struct _VideoAspectFrame
{
    ClutterActor parent;
};

/* Public interface */
GType           video_aspect_frame_get_type     (void) G_GNUC_CONST;
ClutterActor*   video_aspect_frame_new          (void);

#endif // __VIDEO_ASPECT_FRAME_H__
