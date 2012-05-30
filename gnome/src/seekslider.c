/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtk/gtk.h>
#include <string.h>
#include "gtk2_wrappers.h"
#include "seekslider.h"
#include "dbus.h"
#include "logger.h"
#include "calltab.h"

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

static void sfl_seekslider_class_init (SFLSeekSliderClass *klass);
static void sfl_seekslider_init (SFLSeekSlider *seekslider);
static void sfl_seekslider_finalize (GObject *object);
static void sfl_seekslider_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec);
static void sfl_seekslider_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec);

static gboolean on_playback_scale_value_changed_cb(GtkRange *range, GtkScrollType scroll, gdouble value, gpointer user_data);
static gboolean on_playback_scale_pressed_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean on_playback_scale_released_cb(GtkWidget *widget, GdkEventButton *event, gpointer user_data);
static gboolean on_playback_scale_moved_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data);
static gboolean on_playback_scale_scrolled_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data);
static void sfl_seekslider_play_playback_record_cb (GtkButton *button G_GNUC_UNUSED, gpointer user_data);
static void sfl_seekslider_stop_playback_record_cb (GtkButton *button G_GNUC_UNUSED, gpointer user_data);

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
    gboolean can_update_scale;
};

enum
{
    PROP_0,
};

G_DEFINE_TYPE (SFLSeekSlider, sfl_seekslider, GTK_TYPE_HBOX)

static void
sfl_seekslider_class_init (SFLSeekSliderClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);

    object_class->finalize = sfl_seekslider_finalize;

    object_class->set_property = sfl_seekslider_set_property;
    object_class->get_property = sfl_seekslider_get_property;

    g_type_class_add_private (klass, sizeof (SFLSeekSliderPrivate));
}

static void
sfl_seekslider_init (SFLSeekSlider *seekslider)
{
    seekslider->priv = G_TYPE_INSTANCE_GET_PRIVATE (seekslider, SFL_TYPE_SEEKSLIDER, SFLSeekSliderPrivate);

    gdouble init_value = SEEKSLIDER_INIT_VALUE;
    gdouble min_value = SEEKSLIDER_MIN_VALUE;
    gdouble max_value = SEEKSLIDER_MAX_VALUE;
    gdouble stepincrement = SEEKSLIDER_STEPINCREMENT;
    gdouble pageincrement = SEEKSLIDER_PAGEINCREMENT;
    gdouble pagesize = SEEKSLIDER_PAGESIZE;

    GtkAdjustment *adjustment = GTK_ADJUSTMENT(gtk_adjustment_new(init_value, min_value, max_value, stepincrement, pageincrement, pagesize));
    if (adjustment == NULL)
        WARN("Invalid adjustment value for horizontal scale in seekslider");

    seekslider->priv->hscale = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, adjustment);
    if (seekslider->priv->hscale == NULL)
        WARN("Could not create new horizontal scale for seekslider");

    seekslider->priv->hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    if(seekslider->priv->hbox == NULL)
        WARN("Could not create new horizontal box for seekslider");

    seekslider->priv->playRecordImage = gtk_image_new_from_stock(GTK_STOCK_MEDIA_PLAY, GTK_ICON_SIZE_BUTTON);
    seekslider->priv->playRecordWidget = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(seekslider->priv->playRecordWidget), seekslider->priv->playRecordImage);
    if(seekslider->priv->playRecordWidget == NULL)
        WARN("Could not create new playback button for seekslider");

    seekslider->priv->stopRecordImage = gtk_image_new_from_stock(GTK_STOCK_MEDIA_PAUSE, GTK_ICON_SIZE_BUTTON);
    seekslider->priv->stopRecordWidget = gtk_button_new();
    gtk_button_set_image(GTK_BUTTON(seekslider->priv->stopRecordWidget), seekslider->priv->stopRecordImage);
    if(seekslider->priv->stopRecordWidget == NULL)
        WARN("Could not create mew pause button for seekslider");

    seekslider->priv->separator = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    if(seekslider->priv->separator == NULL)
        WARN("Could not create the separator for seekslider");

    seekslider->priv->separatorAlign = gtk_alignment_new(1.0, 0.75, 1.0, 1.0);
    if(seekslider->priv->separatorAlign == NULL)
        WARN("Could not create the separator's alignment");

    g_signal_connect(G_OBJECT(seekslider->priv->hscale), "change-value",
                     G_CALLBACK(on_playback_scale_value_changed_cb), seekslider);

    g_signal_connect_object(G_OBJECT (seekslider->priv->hscale), "button-press-event",
                     G_CALLBACK (on_playback_scale_pressed_cb), seekslider, 0);

    g_signal_connect_object(G_OBJECT (seekslider->priv->hscale), "button-release-event",
                     G_CALLBACK (on_playback_scale_released_cb), seekslider, 0);

    g_signal_connect_object(G_OBJECT (seekslider->priv->hscale), "motion-notify-event",
                     G_CALLBACK (on_playback_scale_moved_cb), seekslider, 0);

    g_signal_connect_object (G_OBJECT (seekslider->priv->hscale), "scroll-event",
                     G_CALLBACK (on_playback_scale_scrolled_cb), seekslider, 0);

    g_object_set(G_OBJECT(seekslider->priv->hscale), "draw-value", FALSE, NULL);

    g_signal_connect_object (G_OBJECT (seekslider->priv->playRecordWidget), "pressed",
                     G_CALLBACK(sfl_seekslider_play_playback_record_cb), seekslider, 0);

    g_signal_connect_object (G_OBJECT (seekslider->priv->stopRecordWidget), "pressed",
                     G_CALLBACK(sfl_seekslider_stop_playback_record_cb), seekslider, 0);

    gtk_container_add(GTK_CONTAINER(seekslider->priv->separatorAlign), seekslider->priv->separator);
    gtk_box_pack_start(GTK_BOX(seekslider->priv->hbox), seekslider->priv->playRecordWidget, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(seekslider->priv->hbox), seekslider->priv->stopRecordWidget, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(seekslider->priv->hbox), seekslider->priv->separatorAlign, FALSE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(seekslider->priv->hbox), seekslider->priv->hscale, TRUE, TRUE, 0);

    gtk_widget_show (seekslider->priv->hbox);
    gtk_widget_show (seekslider->priv->hscale);
    gtk_widget_show (seekslider->priv->separatorAlign);
    gtk_widget_show (seekslider->priv->separator);
    gtk_widget_hide (seekslider->priv->playRecordWidget);
    gtk_widget_hide (seekslider->priv->stopRecordWidget);

    gtk_box_pack_start(GTK_BOX(&seekslider->parent), seekslider->priv->hbox, TRUE, TRUE, 0);

    seekslider->priv->can_update_scale = TRUE;
}

static void
sfl_seekslider_finalize (GObject *object)
{
    SFLSeekSlider *seekslider;

    g_return_if_fail (object != NULL);
    g_return_if_fail (SFL_IS_SEEKSLIDER (object));

    seekslider = SFL_SEEKSLIDER (object);
    g_return_if_fail (seekslider->priv != NULL);

    G_OBJECT_CLASS (sfl_seekslider_parent_class)->finalize (object);
}


static void
sfl_seekslider_set_property (GObject *object, guint prop_id, const GValue *value G_GNUC_UNUSED, GParamSpec *pspec)
{
    switch (prop_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
sfl_seekslider_get_property (GObject *object, guint prop_id, GValue *value G_GNUC_UNUSED, GParamSpec *pspec)
{
    switch (prop_id) {
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
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
    SFLSeekSlider *seekslider;

    seekslider = SFL_SEEKSLIDER (g_object_new (SFL_TYPE_SEEKSLIDER, NULL));

    g_return_val_if_fail (seekslider->priv != NULL, NULL);

    return seekslider;
}

static gboolean
on_playback_scale_value_changed_cb(GtkRange *range G_GNUC_UNUSED, GtkScrollType scroll G_GNUC_UNUSED, gdouble value, gpointer user_data G_GNUC_UNUSED)
{
    dbus_set_record_playback_seek(value);

    return FALSE;
}

static gboolean
on_playback_scale_pressed_cb(GtkWidget *widget G_GNUC_UNUSED, GdkEventButton *event, gpointer user_data)
{
    if (event->button == 1)
        event->button = 2;

    SFLSeekSlider *seekslider = (SFLSeekSlider *)user_data;
    seekslider->priv->can_update_scale = FALSE;

    return FALSE;
}

static gboolean
on_playback_scale_released_cb(GtkWidget *widget G_GNUC_UNUSED, GdkEventButton *event, gpointer user_data)
{
    if (event->button == 1)
        event->button = 2;

    SFLSeekSlider *seekslider = (SFLSeekSlider *)user_data;
    seekslider->priv->can_update_scale = TRUE;

    return FALSE;
}

static gboolean
on_playback_scale_moved_cb(GtkWidget *widget G_GNUC_UNUSED, GdkEvent *event G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
    return FALSE;
}

static gboolean
on_playback_scale_scrolled_cb(GtkWidget *widget G_GNUC_UNUSED, GdkEvent *event G_GNUC_UNUSED, gpointer user_data G_GNUC_UNUSED)
{
    return TRUE;
}

static void sfl_seekslider_play_playback_record_cb (GtkButton *button G_GNUC_UNUSED, gpointer user_data)
{
    SFLSeekSlider *seekslider = (SFLSeekSlider *)user_data;

    callable_obj_t *selectedCall = calltab_get_selected_call(history_tab);
    if (selectedCall == NULL) {
        return;
    }

    DEBUG("Start selected call file playback %s", selectedCall->_recordfile);
    selectedCall->_record_is_playing = dbus_start_recorded_file_playback(selectedCall->_recordfile);

    sfl_seekslider_set_display(seekslider, SFL_SEEKSLIDER_DISPLAY_PAUSE);
}

static void sfl_seekslider_stop_playback_record_cb (GtkButton *button G_GNUC_UNUSED, gpointer user_data)
{
    SFLSeekSlider *seekslider = (SFLSeekSlider *)user_data;

    callable_obj_t *selectedCall = calltab_get_selected_call(history_tab);
    if (selectedCall == NULL) {
        return;
    }

    if (selectedCall) {
        if (selectedCall->_recordfile == NULL || strlen(selectedCall->_recordfile) == 0)
            return;

        if(selectedCall->_record_is_playing) {
            dbus_stop_recorded_file_playback(selectedCall->_recordfile);
            DEBUG("Stop selected call file playback %s", selectedCall->_recordfile);
        }
        selectedCall->_record_is_playing = FALSE;
    }

    sfl_seekslider_set_display(seekslider, SFL_SEEKSLIDER_DISPLAY_PLAY);
}


void sfl_seekslider_update_scale(SFLSeekSlider *seekslider, guint current, guint size)
{
    if (size == 0)
        size = 1;

    if (current > size)
        current = size;

    gdouble val = ((gdouble) current / (gdouble) size) * 100.0;

    if (seekslider->priv->can_update_scale)
        gtk_range_set_value(GTK_RANGE(seekslider->priv->hscale), val);
}

void sfl_seekslider_set_display(SFLSeekSlider *seekslider, SFLSeekSliderDisplay display) {

    // g_return_if_fail (SFL_IS_SEEKSLIDER (seekslider));
    if(seekslider == NULL)
        return;

    switch(display) {
    case SFL_SEEKSLIDER_DISPLAY_PAUSE:
        gtk_widget_hide(seekslider->priv->playRecordWidget);
        gtk_widget_show(seekslider->priv->stopRecordWidget);
        break;
    case SFL_SEEKSLIDER_DISPLAY_PLAY:
        gtk_widget_hide(seekslider->priv->stopRecordWidget);
        gtk_widget_show(seekslider->priv->playRecordWidget);
        break;
    default:
        WARN("Unknown display option for seekslider");
        break;
    }
}

void sfl_seekslider_reset(SFLSeekSlider *seekslider) {
    if(seekslider == NULL)
        return;

    seekslider->priv->can_update_scale = FALSE;
    gtk_range_set_value(GTK_RANGE(seekslider->priv->hscale), 0.0);
    sfl_seekslider_set_display(seekslider, SFL_SEEKSLIDER_DISPLAY_PLAY);
    sfl_seekslider_stop_playback_record_cb (NULL, seekslider);
    seekslider->priv->can_update_scale = TRUE;
}
