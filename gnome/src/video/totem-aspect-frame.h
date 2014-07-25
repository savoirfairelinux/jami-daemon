
/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * mx-aspect-frame.h: A container that respect the aspect ratio of its child
 *
 * Copyright 2010, 2011 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU Lesser General Public License,
 * version 2.1, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin St - Fifth Floor, Boston, MA 02110-1301 USA.
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef __TOTEM_ASPECT_FRAME_H__
#define __TOTEM_ASPECT_FRAME_H__

#include <glib-object.h>
#include <clutter/clutter.h>

G_BEGIN_DECLS

#define TOTEM_TYPE_ASPECT_FRAME totem_aspect_frame_get_type()

#define TOTEM_ASPECT_FRAME(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
  TOTEM_TYPE_ASPECT_FRAME, TotemAspectFrame))

#define TOTEM_ASPECT_FRAME_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST ((klass), \
  TOTEM_TYPE_ASPECT_FRAME, TotemAspectFrameClass))

#define TOTEM_IS_ASPECT_FRAME(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
  TOTEM_TYPE_ASPECT_FRAME))

#define TOTEM_IS_ASPECT_FRAME_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE ((klass), \
  TOTEM_TYPE_ASPECT_FRAME))

#define TOTEM_ASPECT_FRAME_GET_CLASS(obj) \
  (G_TYPE_INSTANCE_GET_CLASS ((obj), \
  TOTEM_TYPE_ASPECT_FRAME, TotemAspectFrameClass))

typedef struct _TotemAspectFrame TotemAspectFrame;
typedef struct _TotemAspectFrameClass TotemAspectFrameClass;
typedef struct _TotemAspectFramePrivate TotemAspectFramePrivate;

struct _TotemAspectFrame
{
  ClutterActor parent;

  TotemAspectFramePrivate *priv;
};

struct _TotemAspectFrameClass
{
  ClutterActorClass parent_class;
};

GType           totem_aspect_frame_get_type     (void) G_GNUC_CONST;

ClutterActor *  totem_aspect_frame_new          (void);

void            totem_aspect_frame_set_child    (TotemAspectFrame *frame,
						 ClutterActor     *child);

void            totem_aspect_frame_set_expand   (TotemAspectFrame *frame,
                                                 gboolean          expand);
gboolean        totem_aspect_frame_get_expand   (TotemAspectFrame *frame);

void            totem_aspect_frame_set_rotation (TotemAspectFrame *frame,
						 gdouble           rotation);
gdouble         totem_aspect_frame_get_rotation (TotemAspectFrame *frame);

G_END_DECLS

#endif /* __TOTEM_ASPECT_FRAME_H__ */
