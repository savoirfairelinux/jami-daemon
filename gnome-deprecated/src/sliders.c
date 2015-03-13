/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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

#include <glib/gi18n.h>
#include "sliders.h"
#include "dbus/dbus.h"
#include "actions.h"
#include <string.h>
#include <gtk/gtk.h>

static GtkWidget * slider[2];
static GtkWidget * button[2];

// icons
static GtkWidget * images[2][4];

enum device_t {
    DEVICE_SPEAKER = 0,
    DEVICE_MIC,
    DEVICE_COUNT
};

enum volume_t {
    MUTED = 0,
    VOL25,
    VOL50,
    VOL75
};

static guint movedConnId[2];   // The slider_moved signal connection ID

static guint device_state = DEVICE_STATE_ACTIVE;

void
update_icons(int dev)
{
    float val = gtk_range_get_value(GTK_RANGE(slider[dev]));

    if (button[dev]) {
        int icon = MUTED;

        if (val == 0 || gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button[dev])))
            icon = MUTED;
        else if (val < 0.33)
            icon = VOL25;
        else if (val < 0.66)
            icon = VOL50;
        else if (val <= 1)
            icon = VOL75;

        gtk_button_set_image(GTK_BUTTON(button[dev]), GTK_WIDGET(images[dev][icon]));
    }
}

void
slider_moved(GtkRange* range, gchar* device)
{
    gdouble slider_value = gtk_range_get_value(range);
    g_debug("Volume changed for %s: %f ", device, slider_value);
    dbus_set_volume(device, slider_value);

    if (g_strcmp0(device, "speaker") == 0)
        update_icons(DEVICE_SPEAKER);
    else
        update_icons(DEVICE_MIC);
}

void
mute_cb(GtkWidget *widget, gchar *device)
{
    int dev;

    gboolean do_mute = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));

    if (g_strcmp0(device, "speaker") == 0) {
        dev = DEVICE_SPEAKER;
        dbus_mute_playback(do_mute);
    } else {
        dev = DEVICE_MIC;
        dbus_mute_capture(do_mute);
    }

    update_icons(dev);
}

void set_slider_value(const gchar *device, gdouble newval)
{
    int dev = 0;

    if (g_strcmp0(device, "speaker") == 0) {
        dev = DEVICE_SPEAKER;
        g_debug("Set value for speaker: %f\n", newval);
    }
    else if (g_strcmp0(device, "mic") == 0) {
        dev = DEVICE_MIC;
        g_debug("Set value for mic: %f\n", newval);
    }
    else {
        g_warning("Unknown device: %s", device);
        return;
    }

    gtk_range_set_value(GTK_RANGE(slider[dev]), newval);

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button[dev]), (newval == 0 ? TRUE: FALSE));

    update_icons(dev);
}

void set_slider_no_update(const gchar * device, gdouble newval)
{
    int dev = 0;

    if (g_strcmp0(device, "speaker") == 0) {
        dev = DEVICE_SPEAKER;
        g_debug("Set value no update for speaker: %f\n", newval);
    } else if (g_strcmp0(device, "mic") == 0) {
        dev = DEVICE_MIC;
        g_debug("Set value no update for mic: %f\n", newval);
    } else {
        g_warning("Unknown device: %s", device);
        return;
    }

    g_signal_handler_block(G_OBJECT(slider[dev]), movedConnId[dev]);
    gtk_range_set_value(GTK_RANGE(slider[dev]), newval);
    g_signal_handler_unblock(slider[dev], movedConnId[dev]);

    update_icons(dev);
}

guint get_mute_unmute_audio_state(void)
{
    return device_state;
}

/** Generates the speaker slider and mute button */
GtkWidget *
create_slider(const gchar * device)
{
    // Increment the references count for the images
    // When the image is removed from a button, if the ref count = 0, then it is destroyed
    // which we don't want ;)

    GtkWidget * ret;
    int dev=0;

    if (g_strcmp0(device, "speaker") == 0) {
        dev = DEVICE_SPEAKER;
        images[DEVICE_SPEAKER][MUTED] = gtk_image_new_from_file(ICONS_DIR "/speaker.svg");
        images[DEVICE_SPEAKER][VOL25] = gtk_image_new_from_file(ICONS_DIR "/speaker_25.svg");
        images[DEVICE_SPEAKER][VOL50] = gtk_image_new_from_file(ICONS_DIR "/speaker_50.svg");
        images[DEVICE_SPEAKER][VOL75] = gtk_image_new_from_file(ICONS_DIR "/speaker_75.svg");
        g_object_ref(images[DEVICE_SPEAKER][MUTED]);
        g_object_ref(images[DEVICE_SPEAKER][VOL25]);
        g_object_ref(images[DEVICE_SPEAKER][VOL50]);
        g_object_ref(images[DEVICE_SPEAKER][VOL75]);
    } else if (g_strcmp0(device, "mic") == 0) {
        dev = DEVICE_MIC;
        images[DEVICE_MIC][MUTED] = gtk_image_new_from_file(ICONS_DIR "/mic.svg");
        images[DEVICE_MIC][VOL25] = gtk_image_new_from_file(ICONS_DIR "/mic_25.svg");
        images[DEVICE_MIC][VOL50] = gtk_image_new_from_file(ICONS_DIR "/mic_50.svg");
        images[DEVICE_MIC][VOL75] = gtk_image_new_from_file(ICONS_DIR "/mic_75.svg");
        g_object_ref(images[DEVICE_MIC][MUTED]);
        g_object_ref(images[DEVICE_MIC][VOL25]);
        g_object_ref(images[DEVICE_MIC][VOL50]);
        g_object_ref(images[DEVICE_MIC][VOL75]);
    }

    ret = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5 /*spacing*/);
    gtk_container_set_border_width(GTK_CONTAINER(ret), 5);

    gtk_widget_set_tooltip_text(GTK_WIDGET(ret),
                                !g_strcmp0(device, "speaker") ? _("Speakers volume") : _("Mic volume"));

    button[dev] = gtk_toggle_button_new();
    gtk_box_pack_start(GTK_BOX(ret), button[dev], FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
    const guint toggledConnId = g_signal_connect(G_OBJECT(button[dev]), "toggled",
                                                 G_CALLBACK(mute_cb), (gpointer) device);

    slider[dev] = gtk_scale_new_with_range(GTK_ORIENTATION_HORIZONTAL, 0, 1, 0.05);
    gtk_scale_set_draw_value(GTK_SCALE(slider[dev]), FALSE);
    //gtk_range_set_update_policy(GTK_RANGE(slider), GTK_UPDATE_DELAYED);
    movedConnId[dev] = g_signal_connect(G_OBJECT(slider[dev]), "value_changed",
                                        G_CALLBACK(slider_moved), (gpointer) device);
    gtk_box_pack_start(GTK_BOX(ret), slider[dev], TRUE /*expand*/, TRUE /*fill*/, 0 /*padding*/);

    set_slider_no_update(device, dbus_get_volume(device));

    g_signal_handler_block(G_OBJECT(button[dev]), toggledConnId);
    const gboolean active =
        (dev == DEVICE_MIC && dbus_is_capture_muted()) ||
        (dev == DEVICE_SPEAKER && dbus_is_playback_muted());

    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button[dev]), active);
    g_signal_handler_unblock(G_OBJECT(button[dev]), toggledConnId);

    return ret;
}
