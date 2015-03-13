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

#include "video_aspect_frame.h"

G_DEFINE_TYPE (VideoAspectFrame, video_aspect_frame, CLUTTER_TYPE_ACTOR)

/* static prototypes */
static void video_aspect_frame_allocate	(ClutterActor *, const ClutterActorBox *, ClutterAllocationFlags );

static void
video_aspect_frame_class_init(VideoAspectFrameClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);
    ClutterActorClass *actor_class = CLUTTER_ACTOR_CLASS(klass);

    /* override allocate method */
    actor_class->allocate = video_aspect_frame_allocate;
}

static void
video_aspect_frame_init(VideoAspectFrame *self)
{
    return;
}

ClutterActor *
video_aspect_frame_new(void)
{
    return g_object_new(VIDEO_ASPECT_FRAME_TYPE, NULL);
}

static void
video_aspect_frame_allocate(ClutterActor           *actor,
                            const ClutterActorBox  *box,
                            ClutterAllocationFlags  flags)
{
    ClutterActor *container, *camera;
    ClutterActorBox container_box;
    gfloat frame_aspect, frame_box_width, frame_box_height,
           camera_aspect, camera_width, camera_height;

    CLUTTER_ACTOR_CLASS(video_aspect_frame_parent_class)->allocate(actor, box, flags);

    /* the first child in the frame is always the camera container */
    container = clutter_actor_get_child_at_index(actor, 0);
    if (!container)
        return;

    /* the first child in the container is always the main camera that must fill the
     * container */
    camera = clutter_actor_get_child_at_index(container, 0);
    if (!camera)
        return;

    /* retrieve the size allocated for the frame box */
    frame_box_width  = box->x2 - box->x1;
    frame_box_height = box->y2 - box->y1;

    /* retrieve the preferred size for the main camera in container box */
    clutter_actor_get_preferred_size(camera, NULL, NULL, &camera_width, &camera_height);

    if (camera_width <= 0.0f || camera_height <= 0.0f)
        return;

    frame_aspect  = frame_box_width / frame_box_height;
    camera_aspect = camera_width / camera_height;

    /* resize the camera actor to fit in the frame box without loosing
     * its aspect ratio */
    if (frame_aspect < camera_aspect) {
        camera_width = frame_box_width;
        camera_height = frame_box_width / camera_aspect;
    } else {
        camera_height = frame_box_height;
        camera_width = frame_box_height * camera_aspect;
    }

    /* center the container box in the space left inside the frame box */
    container_box.x1 = (frame_box_width - camera_width) / 2;
    container_box.y1 = (frame_box_height - camera_height) / 2;
    container_box.x2 = container_box.x1 + camera_width;
    container_box.y2 = container_box.y1 + camera_height;

    /* finally really allocate the container */
    clutter_actor_allocate(container, &container_box, flags);

}
