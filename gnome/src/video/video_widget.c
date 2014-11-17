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
#include "video_aspect_frame.h"
#include "video_renderer.h"
#include "actions.h"

#include "sflphone_client.h"    /* gsettings schema path */
#include "video_window.h"       /* for the window its contained in */

#include <clutter/clutter.h>
#include <clutter-gtk/clutter-gtk.h>

#define VIDEO_LOCAL_HEIGHT              100
#define VIDEO_LOCAL_OPACITY_DEFAULT     150
#define VIDEO_LOCAL_CONSTRAINT_SIZE     "local-constraint-size"
#define VIDEO_LOCAL_CONSTRAINT_POSITION "local-constraint-position"
#define VIDEO_REMOTE_CONSTRAINT_SIZE    "remote-constraint-size"


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
    ClutterActor     *video_aspect_frame;
    VideoScreen      video_screen;
    GHashTable       *video_handles;
    GSettings        *settings;
    gboolean         fullscreen;
    GtkWidget        *video_window;
};

/* Define the VideoWidget type and inherit from GtkWindow */
G_DEFINE_TYPE(VideoWidget, video_widget, GTK_TYPE_BIN);

#define VIDEO_WIDGET_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
            VIDEO_WIDGET_TYPE, VideoWidgetPrivate))

/* static prototypes */
static void       video_widget_draw                     (GtkWidget *);
static GtkWidget* video_widget_draw_screen              (GtkWidget *);
static void       video_widget_redraw_screen            (GtkWidget *);
static VideoArea* video_widget_video_area_get           (GtkWidget *, VIDEO_AREA_ID);
static gboolean   is_video_in_screen                    (GtkWidget *, gchar *);
static Video*     video_widget_retrieve_camera          (GtkWidget *, VIDEO_AREA_ID);
static void       video_widget_add_camera_in_screen     (GtkWidget *, VIDEO_AREA_ID, Video *);
static void       video_widget_remove_camera_in_screen  (GtkWidget *, VIDEO_AREA_ID);
static void       video_widget_show_camera_in_screen    (GtkWidget *, VIDEO_AREA_ID);
static void       video_widget_hide_camera_in_screen    (GtkWidget *, VIDEO_AREA_ID);
static void       cleanup_video_handle                  (gpointer);
static gboolean   on_button_press_in_screen_event_cb    (GtkWidget *, GdkEventButton *, gpointer);
static gboolean   on_pointer_enter_preview_cb           (ClutterActor *, ClutterEvent *, gpointer);
static gboolean   on_pointer_leave_preview_cb           (ClutterActor *, ClutterEvent *, gpointer);
static void       on_drag_data_received_cb              (GtkWidget *, GdkDragContext *, gint, gint, GtkSelectionData *, guint, guint32, gpointer);

/*
 * video_widget_dispose()
 *
 * The dispose function for the video_widget class.
 */
static void
video_widget_dispose(GObject *gobject)
{
    VideoWidget *self = VIDEO_WIDGET(gobject);
    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* In dispose(), you are supposed to free all types referenced from this
     * object which might themselves hold a reference to self. Generally,
     * the most simple solution is to unref all members on which you own a
     * reference.
     */

    /* destory the video window */
    if (priv->video_window)
        gtk_widget_destroy(priv->video_window);
    /* dispose() might be called multiple times, so we must guard against
     * calling g_object_unref() on an invalid GObject by setting the member
     * NULL; g_clear_object() does this for us, atomically.
     */
    g_clear_object(&priv->video_window);

    /* Always chain up to the parent class; there is no need to check if
     * the parent class implements the dispose() virtual function: it is
     * always guaranteed to do so
     */
    G_OBJECT_CLASS(video_widget_parent_class)->dispose(gobject);
}


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
    object_class->dispose = video_widget_dispose;
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
    priv->video_handles = NULL;
    priv->settings = g_settings_new(SFLPHONE_GSETTINGS_SCHEMA);
    priv->fullscreen = FALSE;

    /* init video window where the the video widget will be contained */
    priv->video_window = video_window_new();

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

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* put the video widget into it's window */
    gtk_container_add(GTK_CONTAINER(priv->video_window), self);

    /* This function must be called *after* the video widget is
     * added to its (window) container or else it will result in
     * very high CPU usage; the other alternative is to call
     * gtk_widget_hide() on the video widget before it is added
     * to its container.
     */
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
    GtkWidget *screen;
    ClutterActor *stage;
    ClutterColor stage_color = { 0x00, 0x00, 0x00, 0xff };

    screen = gtk_clutter_embed_new();

    /* create a stage with black background */
    stage = gtk_clutter_embed_get_stage(GTK_CLUTTER_EMBED(screen));
    clutter_actor_set_background_color(stage, &stage_color);

    /* layout manager is used to arrange children in space, here we ask clutter
     * to align children to fill the space when resizing the window */
    clutter_actor_set_layout_manager(stage,
          clutter_bin_layout_new(CLUTTER_BIN_ALIGNMENT_FILL, CLUTTER_BIN_ALIGNMENT_FILL));

    /* add an aspect frame actor to preserve the child actors aspect ratio */
    priv->video_aspect_frame = video_aspect_frame_new();
    clutter_actor_add_child(stage, priv->video_aspect_frame);

    /* add a scene container where we can add and remove our actors */
    priv->video_screen.container = clutter_actor_new();
    clutter_actor_add_child(priv->video_aspect_frame, priv->video_screen.container);

    /* set the minimal size for the window */
    gtk_widget_set_size_request(self, 450, 300);

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
    gfloat aspect_ratio = 0.0f;

    VideoArea *video_area_remote = video_widget_video_area_get(self, VIDEO_AREA_REMOTE);
    VideoArea *video_area_local = video_widget_video_area_get(self, VIDEO_AREA_LOCAL);
    Video *camera_remote = video_widget_retrieve_camera(self, VIDEO_AREA_REMOTE);
    Video *camera_local  = video_widget_retrieve_camera(self, VIDEO_AREA_LOCAL);

    /* Handle the remote camera behaviour */
    if (video_area_remote && video_area_remote->show && camera_remote) {

        /* the remote camera must always fill the container size */
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

            /* clean the previously constraints */
            clutter_actor_clear_constraints(camera_local->texture);

            /* the local camera must always fill the container size */
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

        /* if the video widget is in its window, make it fullscreen */
        GtkWidget *parent = gtk_widget_get_parent(GTK_WIDGET(self));
        if ((gpointer)parent == (gpointer)priv->video_window){
            /* Fullscreen switch on/off */
            priv->fullscreen = !priv->fullscreen;

            if (priv->fullscreen) {
                g_debug("toggle video fullscreen on");
                gtk_window_fullscreen(GTK_WINDOW(priv->video_window));
            } else {
                g_debug("toggle video fullscreen off");
                gtk_window_unfullscreen(GTK_WINDOW(priv->video_window));
            }
        } else {
            /* ignore for now */
            /* TODO: instead, we can put it in the video window
             * and make it fullscreen */
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

    /* show the widget */
    gtk_widget_show(self);

    /* TODO: emit a signal when the video widget should be shown
     * sot that the parent can decide what to do? */

    /* show the parent in certain cases */
    GtkWidget* parent = gtk_widget_get_parent(self);
    if (parent == priv->video_window){
        /* show the video window and its contents */
        gtk_widget_show_all(priv->video_window);
    } else if (parent == NULL) {
        g_debug("video widget has no parent, but video is being rendered");
    }
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

        /* we remove it */
        video_widget_remove_camera_in_screen(self, video_area_id);

        /* and redraw the clutter scene */
        video_widget_redraw_screen(self);
    }

    /* remove the video from the video handle list */
    g_hash_table_remove(priv->video_handles, video_id);

    /* check if there are any videos left */
    if (!g_hash_table_size(priv->video_handles)){
        /* let the parent of the vide widget take care of
         * hiding it when there is no video*/

        /* TODO: emit a signal when the video should be hidden
         * so that the parent can decide what to do? */

        /* hide the parent in certain cases */
        GtkWidget* parent = gtk_widget_get_parent(self);
        if (parent == priv->video_window){
            /* hide the video window */
            gtk_widget_hide(priv->video_window);
        } else if (parent == NULL) {
            g_debug("video widget has no parent");
        }
    }
}


/*
 * video_widget_move_to_preview()
 *
 * This function changes the parent of the video widget to the
 * given preview container. It will be placed into the container
 * via a call to gtk_container_add().
 *
 */
void
video_widget_move_to_preview(GtkWidget *self,
                             GtkContainer *container)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    /* remove video widget from its current container, if it has one */
    GtkWidget* parent = gtk_widget_get_parent(self);
    if ((gpointer)parent != (gpointer)container){
        if (parent != NULL){
            g_object_ref(self);
            gtk_container_remove(GTK_CONTAINER(parent), self);
        }

        gtk_container_add(container, self);

        /* changet the margins */
        gtk_widget_set_margin_left(self, 5);
        gtk_widget_set_margin_right(self, 5);
        gtk_widget_set_margin_top(self, 0);
        gtk_widget_set_margin_bottom(self, 5);
    } else {
        g_debug("video widget already contained in the given container");
    }
}


/*
 * video_widget_move_to_window()
 *
 * This function changes the parent of the video widget to be
 * the standalone video window.
 *
 * The window will be destroyed by the video widget
 */
void
video_widget_move_to_window(GtkWidget *self)
{
    g_return_if_fail(IS_VIDEO_WIDGET(self));

    VideoWidgetPrivate *priv = VIDEO_WIDGET_GET_PRIVATE(self);

    /* remove video widget from its current container, if it has one */
    GtkWidget* parent = gtk_widget_get_parent(self);
    if (parent != priv->video_window) {
         if (parent != NULL){
            g_object_ref(self);
            gtk_container_remove(GTK_CONTAINER(parent), self);
        }

        gtk_container_add(GTK_CONTAINER(priv->video_window), self);

        /* changet the margins */
        gtk_widget_set_margin_left(self, 0);
        gtk_widget_set_margin_right(self, 0);
        gtk_widget_set_margin_top(self, 0);
        gtk_widget_set_margin_bottom(self, 0);
    } else {
        g_debug("video widget already contained in the video window");
    }
}
