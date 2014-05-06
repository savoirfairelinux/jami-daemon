/*
 *  Copyright (C) 2012-2013 Savoir-Faire Linux Inc.
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>
#include <string.h>
#include "seekslider.h"
#include "dbus.h"

/**
 * SECTION:sfl-seekslider
 * @short_description: playback area widgetry
 *
 * The SFLSeekSlider widget displays information about the current playing track
 * (title, album, artist), the elapsed or remaining playback time, and a
 * position slider indicating the playback position.  It translates slider
 * move and drag events into seek requests for the player backend.
 *
 * For shoutcast-style streams, the title/artist/album display is supplemented
 * by metadata extracted from the stream.  See #RBStreamingSource for more information
 * on how the metadata is reported.
 */

#define SEEKSLIDER_INIT_VALUE 0.0
#define SEEKSLIDER_MIN_VALUE 0.0
#define SEEKSLIDER_MAX_VALUE 100.0
#define SEEKSLIDER_STEPINCREMENT 1.0
#define SEEKSLIDER_PAGEINCREMENT 1.0
#define SEEKSLIDER_PAGESIZE 1.0

static void sfl_seekslider_class_init(SFLSeekSliderClass *klass);
static void sfl_seekslider_init(SFLSeekSlider *seekslider);
static void sfl_seekslider_finalize(GObject *object);
static void sfl_seekslider_set_property(GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void sfl_seekslider_get_property(GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static gboolean on_playback_scale_value_changed_cb(GtkRange *range, GtkScrollType scroll, gdouble value, gpointer user_data);
static gboolean on_playback_scale_pressed_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean on_playback_scale_released_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean on_playback_scale_moved_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data);
static gboolean on_playback_scale_scrolled_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data);
static void sfl_seekslider_play_playback_record_cb(GtkButton *button G_GNUC_UNUSED, gpointer user_data);
static void sfl_seekslider_stop_playback_record_cb(GtkButton *button G_GNUC_UNUSED, gpointer user_data);

struct SFLSeekSliderPrivate
{
    GtkWidget *hbox;
    GtkWidget *hscale;
    GtkWidget *playRecordWidget;
    GtkWidget *stopRecordWidget;
    GtkWidget *playRecordImage;
    GtkWidget *stopRecordImage;
    GtkWidget *separator;
    GtkWidget *separatorAlign;
    GtkWidget *timeLabel;
    guint current;
    guint size;
    gboolean is_dragging;
    gboolean can_update_scale;
    gboolean is_playing;
    gchar *file_path;
};

enum
{
    PROP_0,
    PROP_FILE_PATH,
    N_PROPERTIES
};

static GParamSpec *obj_properties[N_PROPERTIES] = { NULL, };

G_DEFINE_TYPE(SFLSeekSlider, sfl_seekslider, GTK_TYPE_HBOX)

static void
sfl_seekslider_class_init(SFLSeekSliderClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = sfl_seekslider_finalize;

    object_class->set_property = sfl_seekslider_set_property;
    object_class->get_property = sfl_seekslider_get_property;

    obj_properties[PROP_FILE_PATH] = g_param_spec_string("file-path", "File path",
            "Set file path for playback", "" /* default value */, G_PARAM_READWRITE);

    g_object_class_install_properties(object_class, N_PROPERTIES, obj_properties);
    g_type_class_add_private(klass, sizeof(SFLSeekSliderPrivate));
}

static void
sfl_seekslider_init(SFLSeekSlider *seekslider)
{
    seekslider->priv = G_TYPE_INSTANCE_GET_PRIVATE(seekslider, SFL_TYPE_SEEKSLIDER, SFLSeekSliderPrivate);

    gdouble init_value =    SEEKSLIDER_INIT_VALUE;
    gdouble min_value =     SEEKSLIDER_MIN_VALUE;
    gdouble max_value =     SEEKSLIDER_MAX_VALUE;
    gdouble stepincrement = SEEKSLIDER_STEPINCREMENT;
    gdouble pageincrement = SEEKSLIDER_PAGEINCREMENT;
    gdouble pagesize =      SEEKSLIDER_PAGESIZE;

    GtkAdjustment *adjustment = GTK_ADJUSTMENT(gtk_adjustment_new(init_value,
                min_value, max_value, stepincrement, pageincrement, pagesize));

    seekslider->priv->hscale = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adjustment);
    seekslider->priv->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);

    seekslider->priv->playRecordImage = gtk_image_new_from_icon_name("media-playback-start-symbolic", GTK_ICON_SIZE_BUTTON);
    seekslider->priv->playRecordWidget = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(seekslider->priv->playRecordWidget), seekslider->priv->playRecordImage);
    seekslider->priv->stopRecordImage = gtk_image_new_from_icon_name("media-playback-pause-symbolic", GTK_ICON_SIZE_BUTTON);

    seekslider->priv->stopRecordWidget = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(seekslider->priv->stopRecordWidget), seekslider->priv->stopRecordImage);

    seekslider->priv->separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    seekslider->priv->timeLabel = gtk_label_new("");
    seekslider->priv->separatorAlign = gtk_alignment_new(1.0, 0.75, 1.0, 1.0);

    g_signal_connect(G_OBJECT(seekslider->priv->hscale), "change-value",
                     G_CALLBACK(on_playback_scale_value_changed_cb), seekslider);

    g_signal_connect_object(G_OBJECT (seekslider->priv->hscale), "button-press-event",
                     G_CALLBACK(on_playback_scale_pressed_cb), seekslider, 0);

    g_signal_connect_object(G_OBJECT (seekslider->priv->hscale), "button-release-event",
                     G_CALLBACK(on_playback_scale_released_cb), seekslider, 0);

    g_signal_connect_object(G_OBJECT (seekslider->priv->hscale), "motion-notify-event",
                     G_CALLBACK(on_playback_scale_moved_cb), seekslider, 0);

    g_signal_connect_object (G_OBJECT (seekslider->priv->hscale), "scroll-event",
                     G_CALLBACK(on_playback_scale_scrolled_cb), seekslider, 0);

    g_object_set(G_OBJECT(seekslider->priv->hscale), "draw-value", FALSE, NULL);

    g_signal_connect_object(G_OBJECT(seekslider->priv->playRecordWidget), "pressed",
                     G_CALLBACK(sfl_seekslider_play_playback_record_cb), seekslider, 0);

    g_signal_connect_object(G_OBJECT(seekslider->priv->stopRecordWidget), "pressed",
                     G_CALLBACK(sfl_seekslider_stop_playback_record_cb), seekslider, 0);

    gtk_container_add(GTK_CONTAINER(seekslider->priv->separatorAlign), seekslider->priv->separator);
    gtk_box_pack_start(GTK_BOX(seekslider->priv->hbox), seekslider->priv->playRecordWidget, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(seekslider->priv->hbox), seekslider->priv->stopRecordWidget, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(seekslider->priv->hbox), seekslider->priv->separatorAlign, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(seekslider->priv->hbox), seekslider->priv->hscale, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(seekslider->priv->hbox), seekslider->priv->timeLabel, FALSE, TRUE, 0);

    gtk_widget_show(seekslider->priv->hbox);
    gtk_widget_show(seekslider->priv->hscale);
    gtk_widget_show(seekslider->priv->separatorAlign);
    gtk_widget_show(seekslider->priv->separator);
    gtk_widget_hide(seekslider->priv->playRecordWidget);
    gtk_widget_hide(seekslider->priv->stopRecordWidget);

    gtk_box_pack_start(GTK_BOX(&seekslider->parent), seekslider->priv->hbox, TRUE, TRUE, 0);

    seekslider->priv->can_update_scale = TRUE;
    seekslider->priv->is_dragging = FALSE;

    seekslider->priv->current = 0;
    seekslider->priv->size = 0;
    seekslider->priv->is_playing = FALSE;
    seekslider->priv->file_path = NULL;
}


static void
ensure_stop(SFLSeekSliderPrivate *priv)
{
    g_return_if_fail(priv && priv->file_path && strlen(priv->file_path) != 0);

    if (priv->is_playing) {
        dbus_stop_recorded_file_playback(priv->file_path);
        g_debug("Stop file playback %s", priv->file_path);
        priv->is_playing = FALSE;
    }
}

static void
sfl_seekslider_finalize(GObject *object)
{
    SFLSeekSlider *seekslider;

    g_return_if_fail(object != NULL);
    g_return_if_fail(SFL_IS_SEEKSLIDER(object));

    seekslider = SFL_SEEKSLIDER(object);

    /* Ensure that we've stopped playback */
    ensure_stop(seekslider->priv);

    G_OBJECT_CLASS(sfl_seekslider_parent_class)->finalize(object);
}


static void
sfl_seekslider_set_property (GObject *object, guint prop_id, const GValue *value G_GNUC_UNUSED, GParamSpec *pspec)
{
    SFLSeekSlider *self = SFL_SEEKSLIDER(object);

    switch (prop_id)
    {
        case PROP_FILE_PATH:
            /* no change */
            if (g_strcmp0(self->priv->file_path, g_value_get_string(value)) == 0)
                break;

            /* cache is_playing as it will be modified */
            const gboolean resume_playing = self->priv->is_playing;
            if (resume_playing)
                sfl_seekslider_stop_playback_record_cb(NULL, self);

            g_free(self->priv->file_path);
            self->priv->file_path = g_value_dup_string(value);
            g_debug("filepath: %s\n", self->priv->file_path);

            if (resume_playing)
                sfl_seekslider_play_playback_record_cb(NULL, self);
            break;

        default:
            /* We don't have any other property... */
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}

static void
sfl_seekslider_get_property (GObject *object, guint prop_id, GValue *value G_GNUC_UNUSED, GParamSpec *pspec)
{
    SFLSeekSlider *self = SFL_SEEKSLIDER(object);

    switch (prop_id)
    {
        case PROP_FILE_PATH:
            g_value_set_string(value, self->priv->file_path);
            break;

        default:
            /* We don't have any other property... */
            G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
            break;
    }
}


/**
 * sfl_seekslider_new:
 * @shell_player: the #RBShellPlayer instance
 * @db: the #RhythmDB instance
 *
 * Creates a new seekslider widget.
 *
 * Return value: the seekslider widget
 */
SFLSeekSlider *
sfl_seekslider_new ()
{
    SFLSeekSlider *seekslider = SFL_SEEKSLIDER(g_object_new(SFL_TYPE_SEEKSLIDER, NULL));

    g_return_val_if_fail(seekslider->priv != NULL, NULL);

    return seekslider;
}

static gboolean
on_playback_scale_value_changed_cb(GtkRange *range G_GNUC_UNUSED, GtkScrollType scroll G_GNUC_UNUSED, gdouble value, gpointer user_data G_GNUC_UNUSED)
{
    SFLSeekSlider *seekslider = SFL_SEEKSLIDER(user_data);

    dbus_set_record_playback_seek(value);

    guint updated_current = (guint) ((seekslider->priv->size * value) / 100.0);
    sfl_seekslider_update_timelabel(seekslider, updated_current, seekslider->priv->size);

    return FALSE;
}

static gboolean
on_playback_scale_pressed_cb(GtkWidget *widget G_GNUC_UNUSED, GdkEventButton *event, gpointer user_data)
{
    if (event->button == 1)
        event->button = 2;

    SFLSeekSlider *seekslider = SFL_SEEKSLIDER(user_data);
    seekslider->priv->can_update_scale = FALSE;
    seekslider->priv->is_dragging = TRUE;

    return FALSE;
}

static gboolean
on_playback_scale_released_cb(GtkWidget *widget G_GNUC_UNUSED, GdkEventButton *event, gpointer user_data)
{
    if (event->button == 1)
        event->button = 2;

    SFLSeekSlider *seekslider = SFL_SEEKSLIDER(user_data);
    seekslider->priv->can_update_scale = TRUE;

    seekslider->priv->is_dragging = FALSE;

    return FALSE;
}

static gboolean
on_playback_scale_moved_cb(GtkWidget *widget, GdkEvent *event G_GNUC_UNUSED, gpointer user_data)
{
    SFLSeekSlider *seekslider = SFL_SEEKSLIDER(user_data);

    if (seekslider->priv->is_dragging == FALSE)
        return FALSE;

    gdouble value = gtk_range_get_value(GTK_RANGE(widget));

    guint updated_current = (guint)(((gdouble)seekslider->priv->size * value) / 100.0);
    seekslider->priv->current = updated_current;

    return FALSE;
}

static gboolean
on_playback_scale_scrolled_cb(GtkWidget *widget G_GNUC_UNUSED, GdkEvent *event G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
    return TRUE;
}

static void sfl_seekslider_play_playback_record_cb (GtkButton *button G_GNUC_UNUSED, gpointer user_data)
{
    SFLSeekSlider *self = SFL_SEEKSLIDER(user_data);

    if (self->priv->file_path == NULL || (*self->priv->file_path == 0))
        return;

    g_debug("Start file playback %s", self->priv->file_path);
    self->priv->is_playing = dbus_start_recorded_file_playback(self->priv->file_path);

    if (self->priv->is_playing)
        sfl_seekslider_set_display(self, SFL_SEEKSLIDER_DISPLAY_PAUSE);
}

static void sfl_seekslider_stop_playback_record_cb(GtkButton *button G_GNUC_UNUSED, gpointer user_data)
{
    SFLSeekSlider *self = SFL_SEEKSLIDER(user_data);

    ensure_stop(self->priv);

    sfl_seekslider_set_display(self, SFL_SEEKSLIDER_DISPLAY_PLAY);
}

void sfl_seekslider_update_timelabel(SFLSeekSlider *seekslider, guint current, guint size)
{
    gchar buf[20];
    guint current_sec = current / 1000;
    guint size_sec = size / 1000;
    guint current_min = current_sec / 60;
    guint size_min = size_sec / 60;
    current_sec = current_sec % 60;
    size_sec = size_sec % 60;

    if (seekslider == NULL)
        return;

    if (size > 0)
        g_snprintf(buf, 20, "%d:%02d / %d:%02d", current_min, current_sec, size_min, size_sec);
    else
        g_snprintf(buf, 20, "%s", "");

    gtk_label_set_text(GTK_LABEL(seekslider->priv->timeLabel), buf);
}

void sfl_seekslider_update_scale(SFLSeekSlider *seekslider, guint current, guint size)
{
    if (seekslider == NULL)
        return;

    if (size == 0)
        size = 1;

    if (current > size)
        current = size;

    gdouble val = ((gdouble) current / (gdouble) size) * 100.0;

    if (seekslider->priv->can_update_scale) {
        gtk_range_set_value(GTK_RANGE(seekslider->priv->hscale), val);
        sfl_seekslider_update_timelabel(seekslider, current, size);
        seekslider->priv->current = current;
        seekslider->priv->size = size;
        if (!seekslider->priv->is_playing) {
            g_warning("Seek slider state is inconsistent, updating icon");
            /* State somehow become inconsistent: the seekbar is moving but
             * the play icon is not set to paused */
            seekslider->priv->is_playing = TRUE;
            sfl_seekslider_set_display(seekslider, SFL_SEEKSLIDER_DISPLAY_PAUSE);
        }
    }
}

void sfl_seekslider_set_display(SFLSeekSlider *seekslider, SFLSeekSliderDisplay display) {

    if (seekslider == NULL || !seekslider->priv ||
        !GTK_IS_WIDGET(seekslider->priv->playRecordWidget) ||
        !GTK_IS_WIDGET(seekslider->priv->stopRecordWidget))
        return;

    switch (display) {
        case SFL_SEEKSLIDER_DISPLAY_PAUSE:
            gtk_widget_hide(seekslider->priv->playRecordWidget);
            gtk_widget_show(seekslider->priv->stopRecordWidget);
            break;
        case SFL_SEEKSLIDER_DISPLAY_PLAY:
            gtk_widget_hide(seekslider->priv->stopRecordWidget);
            gtk_widget_show(seekslider->priv->playRecordWidget);
            break;
        default:
            g_warning("Unknown display option for seekslider");
            break;
    }
}

void sfl_seekslider_reset(SFLSeekSlider *seekslider)
{
    if (seekslider == NULL)
        return;

    seekslider->priv->can_update_scale = FALSE;
    gtk_range_set_value(GTK_RANGE(seekslider->priv->hscale), 0.0);
    sfl_seekslider_set_display(seekslider, SFL_SEEKSLIDER_DISPLAY_PLAY);
    if (seekslider->priv->is_playing)
        sfl_seekslider_stop_playback_record_cb(NULL, seekslider);
    gtk_label_set_text(GTK_LABEL(seekslider->priv->timeLabel), "");
    seekslider->priv->current = 0;
    seekslider->priv->size = 0;
    seekslider->priv->is_playing = FALSE;
    seekslider->priv->can_update_scale = TRUE;
}

gboolean
sfl_seekslider_has_path(SFLSeekSlider *seekslider, const gchar *file_path)
{
    return g_strcmp0(seekslider->priv->file_path, file_path) == 0;
}
