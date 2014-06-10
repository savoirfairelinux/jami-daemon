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
#include "actions.h"

#include "sflphone_client.h"    /* gsettings schema path */

#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>

#define VIDEO_HEIGHT_MIN                200
#define VIDEO_LOCAL_HEIGHT              100
#define VIDEO_LOCAL_OPACITY_DEFAULT     150
#define VIDEO_LOCAL_CONSTRAINT_SIZE     "local-constraint-size"
#define VIDEO_LOCAL_CONSTRAINT_POSITION "local-constraint-position"
#define VIDEO_REMOTE_CONSTRAINT_SIZE    "remote-constraint-size"

#define USE_CONTAINER_HACK CLUTTER_CHECK_VERSION(1, 16, 0) && \
                           !CLUTTER_CHECK_VERSION(1, 18, 0)


typedef struct _Video {
    gchar           *video_id;
    guint           width;
    guint           height;
    ClutterActor    *texture;
    VideoRenderer   *video_renderer;
    gboolean        is_mixer;
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
    GSettings       *settings;
    gboolean        fullscreen;
};

/* Define the VideoWidget type and inherit from GtkWindow */
G_DEFINE_TYPE(VideoWidget, video_widget, GTK_TYPE_WINDOW);

#define VIDEO_WIDGET_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
            VIDEO_WIDGET_TYPE, VideoWidgetPrivate))

/* static prototypes */
static void       video_widget_draw                     (GtkWidget *);
static GtkWidget* video_widget_draw_screen              (GtkWidget *);
static void       video_widget_redraw_screen            (GtkWidget *);
static void       video_widget_set_geometry             (GtkWidget *, guint, guint);
static VideoArea* video_widget_video_area_get           (GtkWidget *, VIDEO_AREA_ID);
static gboolean   is_video_in_screen                    (GtkWidget *, gchar *);
static Video*     video_widget_retrieve_camera          (GtkWidget *, VIDEO_AREA_ID);
static void       video_widget_add_camera_in_screen     (GtkWidget *, VIDEO_AREA_ID, Video *);
static void       video_widget_remove_camera_in_screen  (GtkWidget *, VIDEO_AREA_ID);
static void       video_widget_show_camera_in_screen    (GtkWidget *, VIDEO_AREA_ID);
static void       video_widget_hide_camera_in_screen    (GtkWidget *, VIDEO_AREA_ID);
static void       cleanup_video_handle                  (gpointer);
static gboolean   on_configure_event_cb                 (GtkWidget *, GdkEventConfigure *, gpointer);
static gboolean   on_button_press_in_screen_event_cb    (GtkWidget *, GdkEventButton *, gpointer);
static gboolean   on_pointer_enter_preview_cb           (ClutterActor *, ClutterEvent *, gpointer);
static gboolean   on_pointer_leave_preview_cb           (ClutterActor *, ClutterEvent *, gpointer);
static void       on_drag_data_received_cb              (GtkWidget *, GdkDragContext *, gint, gint, GtkSelectionData *, guint, guint32, gpointer);



/*
 * video_widget_finalize()
 *
 * The finalize function for the video_widget class.
 */
static void
video_widget_finalize(GObject *object)
{
    VideoWidget *self = VIDEO_WIDGET(object);
    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* unref gsettings object */
    g_object_unref(priv->settings);

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
    priv->settings = g_settings_new(SFLPHONE_GSETTINGS_SCHEMA);
    priv->fullscreen = FALSE;

    /* init video_screen */
    priv->video_screen.screen = NULL;
    priv->video_screen.container = NULL;

    /* init video_area */
    VideoArea *video_area = NULL;
    for (int i = 0; i < VIDEO_AREA_LAST; i++) {
        video_area = video_widget_video_area_get(GTK_WIDGET(self), i);
        video_area->show = FALSE;
        video_area->video_id = NULL;
    }

    /* init drag & drop images */
    gtk_drag_dest_set(GTK_WIDGET(self), GTK_DEST_DEFAULT_ALL, NULL, 0, GDK_ACTION_COPY | GDK_ACTION_PRIVATE);
    gtk_drag_dest_add_uri_targets(GTK_WIDGET(self));
    g_signal_connect(GTK_WIDGET(self), "drag-data-received",
            G_CALLBACK(on_drag_data_received_cb), NULL);
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

    g_signal_connect(self, "delete-event",
                     G_CALLBACK(gtk_widget_hide_on_delete), NULL);

    video_widget_draw(self);

    return self;
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

    /* handle configure event */
    g_signal_connect(self, "configure-event",
            G_CALLBACK(on_configure_event_cb),
            NULL);

}


/*
 * video_widget_draw_screen()
 *
 * This function create a new clutter scene in the screen.
 */
static GtkWidget*
video_widget_draw_screen(GtkWidget *self)
{
    g_return_val_if_fail(IS_VIDEO_WIDGET(self), NULL);

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    ClutterActor *stage;
    ClutterColor stage_color = { 0x00, 0x00, 0x00, 0xff };

    GtkWidget *screen = gtk_clutter_embed_new();

    /* create a stage with black background */
    stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(screen));
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

    /* handle button event in screen */
    g_signal_connect(screen, "button-press-event",
            G_CALLBACK(on_button_press_in_screen_event_cb),
            self);

    return screen;
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

    ClutterConstraint *constraint = NULL;
    guint width = 0, height = 0, pos_x = 0, pos_y = 0;
    gdouble aspect_ratio = 0;

    VideoArea *video_area_remote = video_widget_video_area_get(self, VIDEO_AREA_REMOTE);
    VideoArea *video_area_local = video_widget_video_area_get(self, VIDEO_AREA_LOCAL);
    Video *camera_remote = video_widget_retrieve_camera(self, VIDEO_AREA_REMOTE);
    Video *camera_local  = video_widget_retrieve_camera(self, VIDEO_AREA_LOCAL);

    /* retrieve the previous windows settings */
    pos_x  = g_settings_get_int(priv->settings, "video-widget-position-x");
    pos_y  = g_settings_get_int(priv->settings, "video-widget-position-y");

    /* place the window */
    gtk_window_move(GTK_WINDOW(self), pos_x, pos_y);

    /* Handle the remote camera behaviour */
    if (video_area_remote && video_area_remote->show && camera_remote) {

        width = g_settings_get_int(priv->settings, "video-widget-width");
        if (width == 0)
           width = camera_remote->width;

        /* we recalculate the window size based on the camera aspect ratio, so
         * if the remote camera has a different aspect ratio we resize the
         * window height based on the user's preferred width */
        g_assert(camera_remote->width  > 0);
        g_assert(camera_remote->height > 0);
        aspect_ratio = (gdouble) camera_remote->width / camera_remote->height;
        height = width / aspect_ratio;

        /* use the previous setting to set the default size and position */
        gtk_window_set_default_size(GTK_WINDOW(self), width, height);

        /* force aspect ratio preservation and set a minimal size */
        video_widget_set_geometry(self, camera_remote->width, camera_remote->height);

        /* the remote camera must always fit the screen size */
        constraint = clutter_bind_constraint_new(priv->video_screen.container,
                CLUTTER_BIND_SIZE, 0);
        clutter_actor_add_constraint_with_name(camera_remote->texture,
                VIDEO_REMOTE_CONSTRAINT_SIZE, constraint);

        if (camera_remote->is_mixer && video_area_local->show)
            video_widget_hide_camera_in_screen(self, VIDEO_AREA_LOCAL);
        else if (!camera_remote->is_mixer)
            video_widget_show_camera_in_screen(self, VIDEO_AREA_LOCAL);

    }

    /* Handle the local camera behaviour */
    if (video_area_local && video_area_local->show && camera_local) {

        /* if the remote camera is not show, we use all the space for the local camera */
        if (!video_area_remote || !video_area_remote->show) {

            /* use the previous setting to set the default size and position */
            gtk_window_set_default_size(GTK_WINDOW(self), camera_local->width, camera_local->height);

            /* TODO: fix the geometry problem
             * changing the geometry on the fly doesn't work, if we set the geometry here we will have
             * problems later when a remote camera starts...
             * the only workaround so far is to set the geometry only when the remote camera starts,
             * but the local camera will be locked with the same geometry if alone */
            // video_widget_set_geometry(self, camera_local->width, camera_local->height);

            /* clean the previously constraints */
            clutter_actor_clear_constraints(camera_local->texture);

            /* the local camera must always fit the screen size */
            constraint = clutter_bind_constraint_new(priv->video_screen.container,
                    CLUTTER_BIND_SIZE, 0);
            clutter_actor_add_constraint_with_name(camera_local->texture,
                    VIDEO_LOCAL_CONSTRAINT_SIZE, constraint);

        } else {
        /* else the local camera must be resize keeping the aspect ratio and placed */
            aspect_ratio = (gdouble) camera_local->width / camera_local->height;

            /* clean the previously constraints */
            clutter_actor_clear_constraints(camera_local->texture);

            /* Add linear animation for all transformation below */
            clutter_actor_save_easing_state(camera_local->texture);
            clutter_actor_set_easing_mode(camera_local->texture, CLUTTER_LINEAR);

            /* resize the local camera */
            clutter_actor_set_size(camera_local->texture,
                    VIDEO_LOCAL_HEIGHT * aspect_ratio,
                    VIDEO_LOCAL_HEIGHT);

            /* place the local camera in the bottom right corner with a little space */
            constraint = clutter_align_constraint_new(camera_remote->texture,
                    CLUTTER_ALIGN_BOTH, 0.99);
            clutter_actor_add_constraint_with_name(camera_local->texture,
                    VIDEO_LOCAL_CONSTRAINT_POSITION, constraint);

            /* apply an opacity effect on the local camera */
            clutter_actor_set_opacity(camera_local->texture,
                  VIDEO_LOCAL_OPACITY_DEFAULT);

            /* remove animation */
            clutter_actor_restore_easing_state(camera_local->texture);

            /* the actor must react to event */
            clutter_actor_set_reactive(camera_local->texture, TRUE);

            /* handle pointer event on actor */
            g_signal_connect(camera_local->texture,
                             "enter-event",
                             G_CALLBACK(on_pointer_enter_preview_cb),
                             NULL);
            g_signal_connect(camera_local->texture,
                             "leave-event",
                             G_CALLBACK(on_pointer_leave_preview_cb),
                             NULL);
        }

    }


}


/*
 * video_widget_set_geometry
 *
 * This function is use to force the window behaviour to keep a
 * fixed aspect ratio and a minimal size.
 */
static void
video_widget_set_geometry(GtkWidget *self,
                          guint width,
                          guint height)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    /* Set widget geometry behaviour */
    gdouble aspect_ratio = (gdouble) width / height;
    /* The window must keep a fixed aspect ratio */
    GdkGeometry geom = {
        .min_aspect = aspect_ratio,
        .max_aspect = aspect_ratio,
        .min_width  = VIDEO_HEIGHT_MIN * aspect_ratio,
        .min_height = VIDEO_HEIGHT_MIN,
    };
    gtk_window_set_geometry_hints(GTK_WINDOW(self), NULL, &geom,
            GDK_HINT_ASPECT | GDK_HINT_MIN_SIZE);

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
    g_return_val_if_fail(IS_VIDEO_WIDGET(self), NULL);

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
    g_return_val_if_fail(IS_VIDEO_WIDGET(self), FALSE);

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    for (int i = 0; i < VIDEO_AREA_LAST; i++) {
       if (g_strcmp0(priv->video_screen.video_area[i].video_id, video_id) == 0) {
            return TRUE;
       }
    }

    return FALSE;

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
    g_return_val_if_fail(IS_VIDEO_WIDGET(self), NULL);

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

            /* Ensure that the rendering callback is stopped before destroying
             * the actor */
            video_renderer_stop(video->video_renderer);

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
 * video_widget_show_camera_in_screen()
 *
 * This function show the camera contains in the video_area_id.
 * When showing, the camera is being render in the clutter scene
 */
static void
video_widget_show_camera_in_screen(GtkWidget *self,
                                   VIDEO_AREA_ID video_area_id)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* retrieve the camera in the video_area */
    Video *video = video_widget_retrieve_camera(self, video_area_id);
    if (video) {
        /* if the camera is in the stage */
        if (clutter_actor_contains(priv->video_screen.container, video->texture)) {
            /* we show it, the camera will be re-render by clutter */
            clutter_actor_show(video->texture);
        }

        /* show this video_area */
        VideoArea *video_area = video_widget_video_area_get(self, video_area_id);
        if (video_area) {
            video_area->show = TRUE;
        }
    }

}


/*
 * video_widget_hide_camera_in_screen()
 *
 * This function hide the camera contains in the video_area_id.
 * When hiding, the camera stop being render in the clutter scene but don't stop the
 * video_rendering so we can show it back later
 */
static void
video_widget_hide_camera_in_screen(GtkWidget *self,
                                   VIDEO_AREA_ID video_area_id)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* retrieve the camera in the video_area */
    Video *video = video_widget_retrieve_camera(self, video_area_id);
    if (video) {
        /* if the camera is in the stage */
        if (clutter_actor_contains(priv->video_screen.container, video->texture)) {
            /* we hide it, the camera will stop being render by clutter */
            clutter_actor_hide(video->texture);
        }

        /* hide this video_area */
        VideoArea *video_area = video_widget_video_area_get(self, video_area_id);
        if (video_area) {
            video_area->show = FALSE;
        }
    }

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
 * on_drag_data_received_cb()
 *
 * Handle dragged data in the video widget window.
 * Dropping an image causes the client to switch the video input to that image.
 */
static void
on_drag_data_received_cb(G_GNUC_UNUSED GtkWidget *self,
        G_GNUC_UNUSED GdkDragContext *context,
        G_GNUC_UNUSED gint x,
        G_GNUC_UNUSED gint y,
        GtkSelectionData *selection_data,
        G_GNUC_UNUSED guint info,
        G_GNUC_UNUSED guint32 time,
        G_GNUC_UNUSED gpointer data)
{
    gchar **uris = gtk_selection_data_get_uris(selection_data);

    /* We consider only the first selection */
    if (uris && *uris)
       sflphone_switch_video_input(*uris);
    g_strfreev(uris);
}


/*
 * on_configure_event_cb()
 *
 * Handle resizing and moving event in the video windows.
 * This is usefull to store the previous behaviour and restore the user
 * preferences using gsettings.
 */
static gboolean
on_configure_event_cb(GtkWidget *self,
                      GdkEventConfigure *event,
                      G_GNUC_UNUSED gpointer data)
{
    g_return_val_if_fail(IS_VIDEO_WIDGET(self), FALSE);

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* we store the window size on each resize when the remote camera is show */
    VideoArea *video_area = video_widget_video_area_get(self, VIDEO_AREA_REMOTE);
    if (video_area->show) {
       g_settings_set_int(priv->settings, "video-widget-width", event->width);
    }

    /* let the event propagate otherwise the video will not be re-scaled */
    return FALSE;
}


/*
 * on_button_press_in_screen_event_cb()
 *
 * Handle button event in the video screen.
 */
static gboolean
on_button_press_in_screen_event_cb(G_GNUC_UNUSED GtkWidget *widget,
                                   GdkEventButton *event,
                                   gpointer data)
{
    VideoWidget * self = (VideoWidget *) data;

    g_return_val_if_fail(IS_VIDEO_WIDGET(self), FALSE);

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* on double click */
    if (event->type == GDK_2BUTTON_PRESS) {

        /* Fullscreen switch on/off */
        priv->fullscreen = !priv->fullscreen;

        if (priv->fullscreen) {

            gtk_window_fullscreen(GTK_WINDOW(self));

            /* if there is a toolbar we don't want it in the fullscreen,
             * we only care about the video_screen */
            if(priv->toolbar)
                gtk_widget_hide(priv->toolbar);

        } else {

            gtk_window_unfullscreen(GTK_WINDOW(self));

            /* re-show the toolbar */
            if(priv->toolbar)
                gtk_widget_show(priv->toolbar);

        }

    }

    /* the event has been fully handled */
    return TRUE;
}


/*
 * on_pointer_enter_preview_cb()
 *
 * when we are hover the preview camera, we set the opacity to its max
 * value.
 */
static gboolean
on_pointer_enter_preview_cb(ClutterActor *actor,
                            G_GNUC_UNUSED ClutterEvent *event,
                            G_GNUC_UNUSED gpointer data)
{
   /* apply the max opacity */
   clutter_actor_set_opacity(actor, 0xFF);

   return CLUTTER_EVENT_STOP;
}


/*
 * on_pointer_leave_preview_cb()
 *
 * when we leave the preview camera, we restore the opacity to its default
 * value.
 */
static gboolean
on_pointer_leave_preview_cb(ClutterActor *actor,
                            G_GNUC_UNUSED ClutterEvent *event,
                            G_GNUC_UNUSED gpointer data)
{
   /* restore the opacity */
   clutter_actor_set_opacity(actor, VIDEO_LOCAL_OPACITY_DEFAULT);

   return CLUTTER_EVENT_STOP;
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
                          guint height,
                          gboolean is_mixer)
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

    video->is_mixer = is_mixer;

    /* add the video to the video list */
    g_hash_table_insert(priv->video_handles, g_strdup(video_id), video);

    /* add the camera to the screen */
    video_widget_add_camera_in_screen(self, video_area_id, video);

    /* when a new camera start, the screen must be redraw consequently */
    video_widget_redraw_screen(self);

    /* show the widget when at least one camera is started */
    gtk_widget_show_all(self);
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
        return;
    }

    /* if video is draw on screen */
    if (is_video_in_screen(self, video_id)) {

        /* we store the window position with decoration before removing it */
        gint pos_x, pos_y;
        gtk_window_get_position(GTK_WINDOW(self), &pos_x, &pos_y);
        g_settings_set_int(priv->settings, "video-widget-position-x", pos_x);
        g_settings_set_int(priv->settings, "video-widget-position-y", pos_y);

        /* we remove it */
        video_widget_remove_camera_in_screen(self, video_area_id);
        /* and redraw the clutter scene */
        video_widget_redraw_screen(self);
    }

    /* remove the video from the video handle list */
    g_hash_table_remove(priv->video_handles, video_id);

    /* hide the widget when there no video left */
    if (!g_hash_table_size(priv->video_handles))
        gtk_widget_hide(self);

}
