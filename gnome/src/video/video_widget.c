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

#include "video_widget.h"
#include "video_renderer.h"

#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>

#define USE_CONTAINER_HACK CLUTTER_CHECK_VERSION(1, 16, 0) && \
                           !CLUTTER_CHECK_VERSION(1, 18, 0)

typedef struct _VideoArea {
    gboolean        show;
    ClutterActor    *texture;
    gchar           *video_id;
    VideoRenderer   *video_renderer;
} VideoArea;

struct _VideoWidgetPrivate {
    GtkWidget *screen;
    GtkWidget *toolbar;
    ClutterActor *container;
    VideoArea video_area[VIDEO_AREA_LAST];
};

/* Define the VideoWidget type and inherit from GtkWindow */
G_DEFINE_TYPE(VideoWidget, video_widget, GTK_TYPE_WINDOW);

#define VIDEO_WIDGET_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
            VIDEO_WIDGET_TYPE, VideoWidgetPrivate))

static void
video_widget_finalize(GObject *object)
{
    G_OBJECT_CLASS (video_widget_parent_class)->finalize(object);
}


static void
video_widget_class_init(VideoWidgetClass *klass)
{
    /* get the parent gobject class */
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    /* add private structure */
    g_type_class_add_private(klass, sizeof(VideoWidgetPrivate));

    /* override method */
    object_class->finalize = video_widget_finalize;

}

static void
video_widget_init(VideoWidget *self)
{
    VideoWidgetPrivate *priv = self->priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* init clutter */
    int error;
    if ((error = gtk_clutter_init(NULL, NULL)) != CLUTTER_INIT_SUCCESS) {
        g_debug("Could not init clutter : %d\n", error);
    }

    priv->screen = NULL;
    priv->toolbar = NULL;
    priv->container = NULL;
    for (int i = 0; i < VIDEO_AREA_LAST; i++) {
        priv->video_area[i].texture = NULL;
        priv->video_area[i].video_id = NULL;
        priv->video_area[i].video_renderer = NULL;
    }

    gtk_window_set_default_size(GTK_WINDOW(self), VIDEO_WIDGET_WIDTH,
            VIDEO_WIDGET_HEIGHT);

}

static GtkWidget *
video_widget_draw_screen(gint width, gint height)
{
    ClutterActor *stage;
    ClutterColor stage_color = { 0x00, 0x00, 0x00, 0xff };

    GtkWidget *window = gtk_clutter_embed_new();

    /* create a stage with black background */
    stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(window));
    clutter_actor_set_background_color(stage, &stage_color);

    return window;
}

static void
video_widget_redraw_screen(GtkWidget *self)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    ClutterActor *stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(priv->screen));

    ClutterActor *camera_remote = priv->video_area[VIDEO_AREA_REMOTE].texture;
    ClutterActor *camera_local  = priv->video_area[VIDEO_AREA_LOCAL].texture;

    /* window size */
    guint window_width  = gtk_widget_get_allocated_width(self);
    guint window_height = gtk_widget_get_allocated_height(self);

    /* screen size */
    guint screen_width  = gtk_widget_get_allocated_width(priv->screen);
    guint screen_height = gtk_widget_get_allocated_height(priv->screen);

    /* local camera size & position */
    guint local_width   = VIDEO_LOCAL_WIDTH;
    guint local_height  = VIDEO_LOCAL_HEIGHT;
    guint local_x       = screen_width - local_width;
    guint local_y       = screen_height - local_height;

#if !USE_CONTAINER_HACK
    if (!priv->container)
        priv->container = stage;
#else

/* Use an intermediate container in between the textures and the stage.
   This is a workaround for older Clutter (before 1.18), see:
   https://bugzilla.gnome.org/show_bug.cgi?id=711645 */

    if (!priv->container) {
        priv->container = clutter_actor_new();
        clutter_actor_add_constraint(priv->container,
                clutter_bind_constraint_new(stage, CLUTTER_BIND_SIZE, 0.0));
        clutter_actor_add_child(stage, priv->container);
    }
#endif

    /* Handle the remote camera behaviour */
    if (priv->video_area[VIDEO_AREA_REMOTE].show) {

        /* the remote camera must always fit the screen size */
        clutter_actor_set_size(camera_remote, screen_width, screen_height);

        /* if the actor is not already in the stage */
        if (clutter_actor_contains(stage, camera_remote) == FALSE) {
            /* insert the new child under all other actor */
            clutter_actor_insert_child_below(priv->container, camera_remote, NULL);
        } else {
            /* the remote camera must alway been the first child */
            if (clutter_actor_get_first_child(priv->container) != camera_remote) {
                clutter_actor_set_child_below_sibling(priv->container, camera_remote, NULL);
            }
        }

    } else {

        /* if the remote camera exists previously we must remote it from the scene */
        if (camera_remote) {
            if (clutter_actor_contains(priv->container, camera_remote)) {
                clutter_actor_remove_child(priv->container, camera_remote);
            }
            priv->video_area[VIDEO_AREA_REMOTE].texture = NULL;
        }

    }

    /* Handle the local camera behaviour */
    if (priv->video_area[VIDEO_AREA_LOCAL].show) {

        /* the local camera must be placed */
        clutter_actor_set_size(camera_local, local_width, local_height);
        clutter_actor_set_position(camera_local, local_x, local_y);

        if (clutter_actor_contains(priv->container, camera_local) == FALSE) {
            clutter_actor_add_child(priv->container, camera_local);
        }

    } else {

        /* if the local camera exists previously we must remote it from the scene */
        if (camera_local) {
            if (clutter_actor_contains(priv->container, camera_local)) {
                clutter_actor_remove_child(priv->container, camera_local);
            }
            priv->video_area[VIDEO_AREA_LOCAL].texture = NULL;
        }

    }

    /* The window must keep a fixed aspect ratio */
    GdkGeometry geom = {
        .min_aspect = (gdouble) window_width / window_height,
        .max_aspect = (gdouble) window_width / window_height,
    };
    gtk_window_set_geometry_hints(self, NULL, &geom, GDK_HINT_ASPECT);

}

/*
 * video_widget_camera_start()
 *
 * start a new video renderer in the video area selected
 */
void
video_widget_camera_start(GtkWidget *self,
                          VIDEO_AREA_ID video_area_id,
                          gchar *video_id,
                          gchar *shm_path,
                          guint width,
                          guint height)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* store the video_id */
    priv->video_area[video_area_id].video_id = g_strdup(video_id);

    /* create a new texture */
    ClutterActor *stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(priv->screen));
    priv->video_area[video_area_id].texture = clutter_texture_new();

    /* create a new renderer */
    priv->video_area[video_area_id].video_renderer =
        video_renderer_new(priv->video_area[video_area_id].texture, width, height, shm_path);

    /* renderering the shm on the texture */
    if(!video_renderer_run(priv->video_area[video_area_id].video_renderer)) {
        g_object_unref(priv->video_area[VIDEO_AREA_REMOTE].video_renderer);
        g_warning("Could not run video renderer");
    }

    priv->video_area[video_area_id].show = TRUE;

    video_widget_redraw_screen(self);

}

void
video_widget_camera_stop(GtkWidget *self,
                         VIDEO_AREA_ID video_area_id)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    g_free(priv->video_area[video_area_id].video_id);
    priv->video_area[video_area_id].video_id = NULL;

    if (priv->video_area[video_area_id].video_renderer != NULL) {
        video_renderer_stop(priv->video_area[video_area_id].video_renderer);
        priv->video_area[video_area_id].show = FALSE;
        video_widget_redraw_screen(self);
    }
}

static gboolean
on_configure_event_cb(GtkWidget *widget,
                      GdkEventConfigure *event,
                      VideoWidget *self)
{
    g_return_val_if_fail(IS_VIDEO_WIDGET(self), FALSE);

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* redraw the screen on resize */
    video_widget_redraw_screen(self);

    /* return false to let the event propagate */
    return FALSE;
}

static void
video_widget_draw(GtkWidget *self)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    GtkWidget *grid = gtk_grid_new();

    /* Screen box */
    priv->screen = video_widget_draw_screen(400, 350);
    gtk_widget_set_hexpand(priv->screen, TRUE);
    gtk_widget_set_vexpand(priv->screen, TRUE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(priv->screen), 0, 0, 1, 1);

    gtk_container_add(GTK_CONTAINER(self), grid);

    /* handle resize and move event */
    g_signal_connect(self, "configure-event",
            G_CALLBACK(on_configure_event_cb),
            self);
}

GtkWidget *
video_widget_new(void)
{
    GtkWidget *self = g_object_new(VIDEO_WIDGET_TYPE, NULL);
    video_widget_draw(self);

    return self;
}
