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


typedef struct _Video {
    gchar           *video_id;
    guint           width;
    guint           height;
    ClutterActor    *texture;
    VideoRenderer   *video_renderer;
} Video;

typedef struct _VideoArea {
    gchar       *video_id;
    gboolean    show;
} VideoArea;

typedef struct _VideoScreen {
    GtkWidget    *screen;
    ClutterActor *container;
    VideoArea    video_area[VIDEO_AREA_LAST];
} VideoScreen;

struct _VideoWidgetPrivate {
    VideoScreen     video_screen;
    GtkWidget       *toolbar;
    GHashTable      *video_handles;
};

/* Define the VideoWidget type and inherit from GtkWindow */
G_DEFINE_TYPE(VideoWidget, video_widget, GTK_TYPE_WINDOW);

#define VIDEO_WIDGET_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
            VIDEO_WIDGET_TYPE, VideoWidgetPrivate))

static VideoArea* video_widget_video_area_get(GtkWidget *, VIDEO_AREA_ID);


/*
 * video_widget_finalize()
 *
 * The finalize function for the video_widget class.
 */
static void
video_widget_finalize(GObject *object)
{
    G_OBJECT_CLASS (video_widget_parent_class)->finalize(object);
}


/*
 * video_widget_class_init()
 *
 * This function init the video_widget_class.
 */
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


/*
 * video_widget_init()
 *
 * This function init the video_widget.
 * - init clutter
 * - init all the widget members
 */
static void
video_widget_init(VideoWidget *self)
{
    VideoWidgetPrivate *priv = self->priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* init clutter */
    int error;
    if ((error = gtk_clutter_init(NULL, NULL)) != CLUTTER_INIT_SUCCESS) {
        g_debug("Could not init clutter : %d\n", error);
    }

    /* init widget */
    priv->toolbar = NULL;
    priv->video_handles = NULL;

    /* init video_screen */
    priv->video_screen.screen = NULL;
    priv->video_screen.container = NULL;

    /* init video_area */
    VideoArea *video_area = NULL;
    for (int i = 0; i < VIDEO_AREA_LAST; i++) {
        video_area = video_widget_video_area_get(self, i);
        video_area->show = FALSE;
        video_area->video_id = NULL;
    }

}


/*
 * video_widget_draw_screen()
 *
 * This function create a new clutter scene in the screen.
 */
static GtkWidget *
video_widget_draw_screen(GtkWidget *self)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    ClutterActor *stage;
    ClutterColor stage_color = { 0x00, 0x00, 0x00, 0xff };

    GtkWidget *window = gtk_clutter_embed_new();

    /* create a stage with black background */
    stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(window));
    clutter_actor_set_background_color(stage, &stage_color);

#if (USE_CONTAINER_HACK == 0)
    if (!priv->video_screen.container)
        priv->video_screen.container = stage;
#else
/* Use an intermediate container in between the textures and the stage.
   This is a workaround for older Clutter (before 1.18), see:
   https://bugzilla.gnome.org/show_bug.cgi?id=711645 */

    if (!priv->video_screen.container) {
        priv->video_screen.container = clutter_actor_new();
        clutter_actor_add_constraint(priv->video_screen.container,
                clutter_bind_constraint_new(stage, CLUTTER_BIND_SIZE, 0.0));
        clutter_actor_add_child(stage, priv->video_screen.container);
    }
#endif

    return window;
}


/*
 * video_widget_retrieve_camera()
 *
 * This function retrieve the camera currntly show in the video_area_id
 */
static Video*
video_widget_retrieve_camera(GtkWidget *self,
                             VIDEO_AREA_ID video_area_id)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    VideoArea *video_area = video_widget_video_area_get(self, video_area_id);
    if (!video_area || !video_area->video_id)
        return NULL;

    if (!priv->video_handles) {
        g_warning("There is no video started currenlty !\n");
        return NULL;
    }

    Video *video = g_hash_table_lookup(priv->video_handles, video_area->video_id);
    if (!video)
        g_warning("This video doesn't exist !\n");

    return video;

}


/*
 * video_widget_resize_screen()
 *
 * This function is use to resize the different actor in the clutter scene
 * when the screen size has change.
 */
static void
video_widget_resize_screen(GtkWidget *self, guint width, guint height)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    VideoArea *video_area = NULL;
    Video *camera_remote = NULL;
    Video *camera_local  = NULL;

    if (!priv->video_handles)
        return;

    guint toolbar_height = 0;

    if (priv->toolbar)
        toolbar_height = gtk_widget_get_allocated_height(priv->toolbar);

    /* Handle the remote camera behaviour */
    video_area = video_widget_video_area_get(self, VIDEO_AREA_REMOTE);
    if (video_area &&
            video_area->show &&
            (camera_remote = video_widget_retrieve_camera(self, VIDEO_AREA_REMOTE))) {

        /* the remote camera must always fit the screen size */
        clutter_actor_set_size(camera_remote->texture, width, height - toolbar_height);

    }

    /* Handle the local camera behaviour */
    video_area = video_widget_video_area_get(self, VIDEO_AREA_LOCAL);
    if (video_area &&
            video_area->show &&
            (camera_local = video_widget_retrieve_camera(self, VIDEO_AREA_LOCAL))) {

        /* if the remote camera is not show, we use all the space for the local camera */
        if (!(video_area = video_widget_video_area_get(self, VIDEO_AREA_REMOTE)) ||
                !video_area->show) {

            clutter_actor_set_size(camera_local->texture, width, height - toolbar_height);

        } else {
        /* else the local camera must be resize keeping the aspect ratio and placed */

            gdouble aspect_ratio = (gdouble) camera_local->width / camera_local->height;

            clutter_actor_set_size(camera_local->texture,
                    VIDEO_LOCAL_HEIGHT * aspect_ratio,
                    VIDEO_LOCAL_HEIGHT);
            clutter_actor_set_position(
                    camera_local->texture,
                    width - (VIDEO_LOCAL_HEIGHT * aspect_ratio),
                    height - VIDEO_LOCAL_HEIGHT - toolbar_height);

        }

    }

}


/*
 * video_widget_retrieve_screen_size()
 *
 * This function retrieve the screen size base on the camera size
 * if the remote camera is show we alway size the screen base on its values.
 */
static void
video_widget_retrieve_screen_size(GtkWidget *self,
                                  guint *width,
                                  guint *height)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    VideoArea *video_area = NULL;
    Video *camera = NULL;

    *width  = 0;
    *height = 0;

    if ((video_area = video_widget_video_area_get(self, VIDEO_AREA_REMOTE)) &&
         video_area->show &&
         (camera = video_widget_retrieve_camera(self, VIDEO_AREA_REMOTE))) {

        *width  = camera->width;
        *height = camera->height;

    } else if ((video_area = video_widget_video_area_get(self, VIDEO_AREA_LOCAL)) &&
                video_area->show &&
                (camera = video_widget_retrieve_camera(self, VIDEO_AREA_LOCAL))) {

        *width  = camera->width;
        *height = camera->height;

    }

}


/*
 * video_widget_set_aspect_ratio
 *
 * This function is use to force the window behaviour to keep a
 * fixed aspect ratio.
 */
static void
video_widget_set_aspect_ratio(GtkWidget *self,
                              guint width,
                              guint height)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* Set widget geometry behaviour */
    gdouble aspect_ratio = (gdouble) width / height;
    /* The window must keep a fixed aspect ratio */
    GdkGeometry geom = {
        .min_aspect = aspect_ratio,
        .max_aspect = aspect_ratio,
    };
    gtk_window_set_geometry_hints(priv->video_screen.screen, NULL, &geom, GDK_HINT_ASPECT);

}


/*
 * video_widget_redraw_screen()
 *
 * This function is use to redraw the clutter scene after a camera have been add
 * or remove
 */
static void
video_widget_redraw_screen(GtkWidget *self)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    guint width  = 0;
    guint height = 0;

    video_widget_retrieve_screen_size(self, &width, &height);

    video_widget_set_aspect_ratio(self, width, height);

    VideoArea *video_area_remote = video_widget_video_area_get(self, VIDEO_AREA_REMOTE);
    VideoArea *video_area_local = video_widget_video_area_get(self, VIDEO_AREA_LOCAL);
    Video *camera_remote = video_widget_retrieve_camera(self, VIDEO_AREA_REMOTE);
    Video *camera_local  = video_widget_retrieve_camera(self, VIDEO_AREA_LOCAL);

    /* Handle the remote camera behaviour */
    if (video_area_remote && video_area_remote->show && camera_remote) {

        /* the remote camera must always fit the screen size */
        clutter_actor_set_size(camera_remote->texture, width, height);

    }

    /* Handle the local camera behaviour */
    if (video_area_local && video_area_local->show && camera_local) {

        /* if the remote camera is not show, we use all the space for the local camera */
        if (!video_area_remote || !video_area_remote->show) {
            clutter_actor_set_size(camera_local->texture, width, height);
        } else {
        /* else the local camera must be resize keeping the aspect ratio and placed */
            gdouble aspect_ratio = (gdouble) camera_local->width / camera_local->height;

            clutter_actor_set_size(camera_local->texture,
                    VIDEO_LOCAL_HEIGHT * aspect_ratio,
                    VIDEO_LOCAL_HEIGHT);
            clutter_actor_set_position(
                    camera_local->texture,
                    width - (VIDEO_LOCAL_HEIGHT * aspect_ratio),
                    height - VIDEO_LOCAL_HEIGHT);
        }

    }

    gtk_widget_set_size_request(priv->video_screen.screen, width, height);

}


/*
 * video_widget_video_area_get()
 *
 * Getter to the video_area at the video_area_id
 */
static VideoArea*
video_widget_video_area_get(GtkWidget *self,
                            VIDEO_AREA_ID video_area_id)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    if (video_area_id < 0 || video_area_id >= VIDEO_AREA_LAST) {
        return NULL;
    }

    return &(priv->video_screen.video_area[video_area_id]);
}


/*
 * is_video_in_screen()
 *
 * This function is an helper to know if a video_id is currently show in the
 * clutter scene
 */
static gboolean
is_video_in_screen(GtkWidget *self,
                   gchar *video_id)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    for (int i = 0; i < VIDEO_AREA_LAST; i++) {
       if (g_strcmp0(priv->video_screen.video_area[i].video_id, video_id) == 0) {
            return TRUE;
       }
    }

    return FALSE;

}

/*
 * cleanup_video_handle()
 *
 * This function is the destroyer function called when removing a key from the
 * video_handles hastable.
 */
static void
cleanup_video_handle(gpointer data)
{
    Video *v = (Video *) data;
    if (!v)
        return;

    /* FIXME: segfault when auto call */
    /* we can stop the video rendering here */
    /* if (v->video_renderer) */
    /*     video_renderer_stop(v->video_renderer); */

    g_free(v->video_id);

    g_free(v);
}


/*
 * video_widget_remove_camera_in_screen()
 *
 * This function remove from the clutter scene the camera currently show in
 * the video_area_id and free this video_area.
 */
static void
video_widget_remove_camera_in_screen(GtkWidget *self,
                                     VIDEO_AREA_ID video_area_id)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* retrieve the camera in the video_area */
    Video *video = video_widget_retrieve_camera(self, video_area_id);
    if (video) {
        /* if the camera is in the stage */
        if (clutter_actor_contains(priv->video_screen.container, video->texture)) {
            /* we remove it */
            clutter_actor_remove_child(priv->video_screen.container, video->texture);
        }

        /* free this video_area */
        VideoArea *video_area = video_widget_video_area_get(self, video_area_id);
        if (video_area) {
            g_free(video_area->video_id);
            video_area->video_id = NULL;
            video_area->show = FALSE;
        }

    }

}


/*
 * video_widget_add_camera_in_screen()
 *
 * This function add to the clutter scene the video in the video_area selected.
 * - remove the camera allocated to this video_area
 * - insert the new camera in the clutter scene
 * - inform the video_area to use this camera
 */
static void
video_widget_add_camera_in_screen(GtkWidget *self,
                                  VIDEO_AREA_ID video_area_id,
                                  Video *video)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* if there was already a camera in this video_area,
     * we must remove it from the screen */
    video_widget_remove_camera_in_screen(self, video_area_id);

    /* insert the new actor in the stage */
    if (video_area_id == VIDEO_AREA_REMOTE)
        clutter_actor_insert_child_below(priv->video_screen.container, video->texture, NULL);
    else
        clutter_actor_insert_child_above(priv->video_screen.container, video->texture, NULL);

    /* fill this video_area with the new camera */
    VideoArea *video_area = video_widget_video_area_get(self, video_area_id);
    if (video_area) {
        video_area->video_id = g_strdup(video->video_id);
        video_area->show = TRUE;
    }

}


/*
 * video_widget_camera_start()
 *
 * This function add the logic to start a new video and show it in the scene.
 * - create a new video
 * - start a new video_renderer for this video in the video_area selected
 * - add the video to the video_handles
 * - add the video in screen
 * - redraw the screen
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

    if (!priv->video_handles)
        priv->video_handles = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, cleanup_video_handle);

    /* create a new empty video */
    Video *video = g_new0(Video, 1);

    /* store the video informations */
    video->width    = width;
    video->height   = height;
    video->video_id = g_strdup(video_id);

    /* create a new texture */
    video->texture = clutter_texture_new();

    /* renderer the shm inside the texture */
    video->video_renderer = video_renderer_new(video->texture, width, height, shm_path);
    if(!video_renderer_run(video->video_renderer)) {
        g_object_unref(video->video_renderer);
        g_warning("Could not run video renderer");
    }

    /* add the video to the video list */
    g_hash_table_insert(priv->video_handles, g_strdup(video_id), video);

    video_widget_add_camera_in_screen(self, video_area_id, video);

    /* when a new camera start, the screen must be redraw consequently */
    video_widget_redraw_screen(self);

}


/*
 * video_widget_camera_stop()
 *
 * This function add the logic to remove a video from the video_handles and the
 * scene if need.
 */
void
video_widget_camera_stop(GtkWidget *self,
                         VIDEO_AREA_ID video_area_id,
                         gchar *video_id)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    Video *video = NULL;

    if (!video_id ||
            !(video = g_hash_table_lookup(priv->video_handles, video_id))) {
        g_warning("Video with id %s is not in the handle video list", video_id);
        return NULL;
    }

    /* if video is draw on screen */
    if (is_video_in_screen(self, video_id)) {
        /* we remove it */
        video_widget_remove_camera_in_screen(self, video_area_id);
        /* and redraw the clutter scene */
        video_widget_redraw_screen(self);
    }

    /* remove the video from the video handle list */
    g_hash_table_remove(priv->video_handles, video_id);

}


/*
 * on_configure_event_cb()
 *
 * Callback associated to resize and move events
 */
static gboolean
on_configure_event_cb(GtkWidget *widget,
                      GdkEventConfigure *event,
                      VideoWidget *self)
{
    g_return_val_if_fail(IS_VIDEO_WIDGET(self), FALSE);

    /* redraw the screen on resize */
    video_widget_resize_screen(self, event->width, event->height);

    /* return false to let the event propagate */
    return FALSE;
}


/*
 * video_widget_draw()
 *
 * The main widget draw function.
 */
static void
video_widget_draw(GtkWidget *self)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    GtkWidget *grid = gtk_grid_new();

    /* Screen box */
    priv->video_screen.screen = video_widget_draw_screen(self);
    gtk_widget_set_hexpand(priv->video_screen.screen, TRUE);
    gtk_widget_set_vexpand(priv->video_screen.screen, TRUE);
    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(priv->video_screen.screen), 0, 0, 1, 1);

    gtk_container_add(GTK_CONTAINER(self), grid);

    /* handle resize and move event */
    g_signal_connect(self, "configure-event",
            G_CALLBACK(on_configure_event_cb),
            self);
}


/*
 * video_widget_new()
 *
 * The function use to create a new video_widget
 */
GtkWidget*
video_widget_new(void)
{
    GtkWidget *self = g_object_new(VIDEO_WIDGET_TYPE, NULL);

    video_widget_draw(self);

    return self;
}
