/* -*- mode: C; c-file-style: "gnu"; indent-tabs-mode: nil; -*- */
/*
 * A container that respects the aspect ratio of its child
 *
 * Copyright 2010, 2011 Intel Corporation.
 * Copyright 2012, Red Hat, Inc.
 *
 * Based upon mx-aspect-frame.c
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
 */

#include <math.h>

#include "totem-aspect-frame.h"

G_DEFINE_TYPE (TotemAspectFrame, totem_aspect_frame, CLUTTER_TYPE_ACTOR)

#define ASPECT_FRAME_PRIVATE(o)                         \
  (G_TYPE_INSTANCE_GET_PRIVATE ((o),                    \
                                TOTEM_TYPE_ASPECT_FRAME,   \
                                TotemAspectFramePrivate))

enum
{
  PROP_0,

  PROP_EXPAND,
};

struct _TotemAspectFramePrivate
{
  guint expand : 1;
  gdouble rotation;
};


static void
totem_aspect_frame_get_property (GObject    *object,
                                 guint       property_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  TotemAspectFrame *frame = TOTEM_ASPECT_FRAME (object);

  switch (property_id)
    {
    case PROP_EXPAND:
      g_value_set_boolean (value, totem_aspect_frame_get_expand (frame));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
totem_aspect_frame_set_property (GObject      *object,
                                 guint         property_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  switch (property_id)
    {
    case PROP_EXPAND:
      totem_aspect_frame_set_expand (TOTEM_ASPECT_FRAME (object),
                                   g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
    }
}

static void
totem_aspect_frame_dispose (GObject *object)
{
  G_OBJECT_CLASS (totem_aspect_frame_parent_class)->dispose (object);
}

static void
totem_aspect_frame_finalize (GObject *object)
{
  G_OBJECT_CLASS (totem_aspect_frame_parent_class)->finalize (object);
}

static void
totem_aspect_frame_get_preferred_width (ClutterActor *actor,
                                        gfloat        for_height,
                                        gfloat       *min_width_p,
                                        gfloat       *nat_width_p)
{
  gboolean override;

  if (for_height >= 0)
    override = FALSE;
  else
    g_object_get (G_OBJECT (actor), "natural-height-set", &override, NULL);

  if (override)
    g_object_get (G_OBJECT (actor), "natural-height", &for_height, NULL);

  CLUTTER_ACTOR_CLASS (totem_aspect_frame_parent_class)->
    get_preferred_width (actor, for_height, min_width_p, nat_width_p);
}

static void
totem_aspect_frame_get_preferred_height (ClutterActor *actor,
                                         gfloat        for_width,
                                         gfloat       *min_height_p,
                                         gfloat       *nat_height_p)
{
  gboolean override;

  if (for_width >= 0)
    override = FALSE;
  else
    g_object_get (G_OBJECT (actor), "natural-width-set", &override, NULL);

  if (override)
    g_object_get (G_OBJECT (actor), "natural-width", &for_width, NULL);

  CLUTTER_ACTOR_CLASS (totem_aspect_frame_parent_class)->
    get_preferred_height (actor, for_width, min_height_p, nat_height_p);
}

static void
totem_aspect_frame_get_size (TotemAspectFrame *frame,
                             gdouble           rotation,
                             gfloat           *width,
                             gfloat           *height)
{
  ClutterActorBox box;
  gfloat w, h;

  clutter_actor_get_allocation_box (CLUTTER_ACTOR (frame), &box);

  if (fmod (rotation, 180.0) == 90.0)
    {
      w = box.y2 - box.y1;
      h = box.x2 - box.x1;
    }
  else
    {
      w = box.x2 - box.x1;
      h = box.y2 - box.y1;
    }

  if (width)
    *width = w;
  if (height)
    *height = h;
}

static void
_get_allocation (ClutterActor *actor,
                 gfloat       *width,
                 gfloat       *height)
{
  ClutterActorBox box;

  clutter_actor_get_allocation_box (actor, &box);

  if (width)
    *width = box.x2 - box.x1;
  if (height)
    *height = box.y2 - box.y1;
}

static void
totem_aspect_frame_set_rotation_internal (TotemAspectFrame *frame,
					  gdouble           rotation,
					  gboolean          animate)
{
  TotemAspectFramePrivate *priv = frame->priv;
  ClutterActor *actor;
  gfloat frame_width, frame_height;
  gfloat child_width, child_height;
  gfloat child_dest_width, child_dest_height;
  gdouble frame_aspect;
  gdouble child_aspect;

  actor = clutter_actor_get_child_at_index (CLUTTER_ACTOR (frame), 0);
  if (!actor)
    return;

  totem_aspect_frame_get_size (frame, rotation,
                               &frame_width, &frame_height);
  _get_allocation (actor, &child_width, &child_height);

  if (child_width <= 0.0f || child_height <= 0.0f)
    return;

  frame_aspect = frame_width / frame_height;
  child_aspect = child_width / child_height;

  if ((frame_aspect < child_aspect) ^ priv->expand)
    {
      child_dest_width = frame_width;
      child_dest_height = frame_width / child_aspect;
    }
  else
    {
      child_dest_height = frame_height;
      child_dest_width = frame_height * child_aspect;
    }

  clutter_actor_set_pivot_point (actor, 0.5, 0.5);

  if (animate)
    {
      clutter_actor_save_easing_state (actor);
      clutter_actor_set_easing_duration (actor, 500);
    }

  clutter_actor_set_rotation_angle (actor, CLUTTER_Z_AXIS, rotation);
  clutter_actor_set_scale (actor,
                           child_dest_width / child_width,
                           child_dest_height / child_height);

  if (animate)
    clutter_actor_restore_easing_state (actor);
}

static void
totem_aspect_frame_allocate (ClutterActor           *actor,
                             const ClutterActorBox  *box,
                             ClutterAllocationFlags  flags)
{
  ClutterActor *child;
  ClutterActorBox child_box;
  gfloat aspect, child_aspect, width, height, box_width, box_height;

  TotemAspectFramePrivate *priv = TOTEM_ASPECT_FRAME (actor)->priv;

  CLUTTER_ACTOR_CLASS (totem_aspect_frame_parent_class)->
    allocate (actor, box, flags);

  child = clutter_actor_get_child_at_index (actor, 0);
  if (!child)
    return;

  box_width = box->x2 - box->x1;
  box_height = box->y2 - box->y1;

  clutter_actor_get_preferred_size (child, NULL, NULL, &width, &height);

  if (width <= 0.0f || height <= 0.0f)
    return;

  aspect = box_width / box_height;
  child_aspect = width / height;

  if ((aspect < child_aspect) ^ priv->expand)
    {
      width = box_width;
      height = box_width / child_aspect;
    }
  else
    {
      height = box_height;
      width = box_height * child_aspect;
    }

  child_box.x1 = (box_width - width) / 2;
  child_box.y1 = (box_height - height) / 2;
  child_box.x2 = child_box.x1 + width;
  child_box.y2 = child_box.y1 + height;

  clutter_actor_allocate (child, &child_box, flags);

  totem_aspect_frame_set_rotation_internal (TOTEM_ASPECT_FRAME (actor),
                                            priv->rotation, FALSE);
}

static void
totem_aspect_frame_paint (ClutterActor *actor)
{
  ClutterActor *child;
  TotemAspectFramePrivate *priv = TOTEM_ASPECT_FRAME (actor)->priv;

  child = clutter_actor_get_child_at_index (actor, 0);

  if (!child)
    return;

  if (priv->expand)
    {
      gfloat width, height;

      clutter_actor_get_size (actor, &width, &height);

      cogl_clip_push_rectangle (0.0, 0.0, width, height);
      clutter_actor_paint (child);
      cogl_clip_pop ();
    }
  else
    clutter_actor_paint (child);
}

static void
totem_aspect_frame_pick (ClutterActor       *actor,
                         const ClutterColor *color)
{
  ClutterActorBox box;
  ClutterActor *child;
  TotemAspectFramePrivate *priv = TOTEM_ASPECT_FRAME (actor)->priv;

  clutter_actor_get_allocation_box (actor, &box);

  CLUTTER_ACTOR_CLASS (totem_aspect_frame_parent_class)->pick (actor, color);

  child = clutter_actor_get_child_at_index (actor, 0);

  if (!child)
    return;

  if (priv->expand)
    {
      cogl_clip_push_rectangle (0.0, 0.0, box.x2 - box.x1, box.y2 - box.y1);
      clutter_actor_paint (child);
      cogl_clip_pop ();
    }
  else
    clutter_actor_paint (child);
}

static void
totem_aspect_frame_class_init (TotemAspectFrameClass *klass)
{
  GParamSpec *pspec;

  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS (klass);

  g_type_class_add_private (klass, sizeof (TotemAspectFramePrivate));

  /* object_class->get_property = totem_aspect_frame_get_property; */
  /* object_class->set_property = totem_aspect_frame_set_property; */
  /* object_class->dispose = totem_aspect_frame_dispose; */
  /* object_class->finalize = totem_aspect_frame_finalize; */

  /* actor_class->get_preferred_width = totem_aspect_frame_get_preferred_width; */
  /* actor_class->get_preferred_height = totem_aspect_frame_get_preferred_height; */
  actor_class->allocate = totem_aspect_frame_allocate;
  /* actor_class->paint = totem_aspect_frame_paint; */
  /* actor_class->pick = totem_aspect_frame_pick; */

  pspec = g_param_spec_boolean ("expand",
                                "Expand",
                                "Fill the allocated area with the child and "
                                "clip off the excess.",
                                FALSE,
                                G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  g_object_class_install_property (object_class, PROP_EXPAND, pspec);
}

static void
totem_aspect_frame_init (TotemAspectFrame *self)
{
  self->priv = ASPECT_FRAME_PRIVATE (self);
  clutter_actor_set_pivot_point (CLUTTER_ACTOR (self), 0.5f, 0.5f);
}

ClutterActor *
totem_aspect_frame_new (void)
{
  return g_object_new (TOTEM_TYPE_ASPECT_FRAME, NULL);
}

void
totem_aspect_frame_set_expand (TotemAspectFrame *frame, gboolean expand)
{
  TotemAspectFramePrivate *priv;

  g_return_if_fail (TOTEM_IS_ASPECT_FRAME (frame));

  priv = frame->priv;
  if (priv->expand != expand)
    {
      priv->expand = expand;
      g_object_notify (G_OBJECT (frame), "expand");

      totem_aspect_frame_set_rotation_internal (frame, priv->rotation, TRUE);
    }
}

gboolean
totem_aspect_frame_get_expand (TotemAspectFrame *frame)
{
  g_return_val_if_fail (TOTEM_IS_ASPECT_FRAME (frame), FALSE);
  return frame->priv->expand;
}

void
totem_aspect_frame_set_child   (TotemAspectFrame *frame,
				ClutterActor     *child)
{
  g_return_if_fail (TOTEM_IS_ASPECT_FRAME (frame));

  clutter_actor_add_child (CLUTTER_ACTOR (frame), child);
}

void
totem_aspect_frame_set_rotation (TotemAspectFrame *frame,
				 gdouble           rotation)
{
  g_return_if_fail (TOTEM_IS_ASPECT_FRAME (frame));
  g_return_if_fail (fmod (rotation, 90.0) == 0.0);

  rotation = fmod (rotation, 360.0);

  /* When animating, make sure that we go in the right direction,
   * otherwise we'll spin in the wrong direction going back to 0 from 270 */
  if (rotation == 0.0 && frame->priv->rotation == 270.0)
    rotation = 360.0;
  else if (rotation == 90.0 && frame->priv->rotation == 360.0)
    totem_aspect_frame_set_rotation_internal (frame, 0.0, FALSE);
  else if (rotation == 270.0 && fmod (frame->priv->rotation, 360.0) == 0.0)
    totem_aspect_frame_set_rotation_internal (frame, 360.0, FALSE);

  g_debug ("Setting rotation to '%lf'", rotation);

  frame->priv->rotation = rotation;
  totem_aspect_frame_set_rotation_internal (frame, rotation, TRUE);
}

gdouble
totem_aspect_frame_get_rotation (TotemAspectFrame *frame)
{
  gdouble rotation;

  g_return_val_if_fail (TOTEM_IS_ASPECT_FRAME (frame), 0.0);

  rotation = fmod (frame->priv->rotation, 360.0);
  g_debug ("Got rotation %lf", rotation);

  return rotation;
}
