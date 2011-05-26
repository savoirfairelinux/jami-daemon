/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Tristan Matthews <tristan.matthews@savoirfairelinux.com>
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
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
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


#include <videoconf.h>
#include <utils.h>
#include <string.h>
#include <eel-gconf-extensions.h>
#include "dbus/dbus.h"

    
void active_is_always_recording (void)
{
    gboolean enabled = FALSE;

    enabled = dbus_get_is_always_recording();

    if(enabled) {
        enabled = FALSE;
    }
    else {
        enabled = TRUE;
    }

    dbus_set_is_always_recording(enabled);
}


static void record_path_changed (GtkFileChooser *chooser , GtkLabel *label UNUSED)
{
    DEBUG ("record_path_changed");

    gchar* path;
    path = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
    DEBUG ("path2 %s", path);
    dbus_set_record_path (path);
}

GtkWidget* create_video_configuration()
{
    // Main widget
    GtkWidget *ret;
    // Sub boxes
    GtkWidget *frame;

    ret = gtk_vbox_new (FALSE, 10);
    gtk_container_set_border_width (GTK_CONTAINER (ret), 10);

    GtkWidget *table;

    gnome_main_section_new_with_table (_ ("Video Manager"), &frame, &table, 1, 4);
    gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);

    //int video_manager = dbus_get_video_manager();

    // Recorded file saving path
    GtkWidget *label;
    GtkWidget *folderChooser;
    gchar *dftPath;

    /* Get the path where to save video files */
    dftPath = dbus_get_record_path ();
    DEBUG ("VideoConf: Load recording path %s", dftPath);

    gnome_main_section_new_with_table (_ ("Recordings"), &frame, &table, 2, 3);
    gtk_box_pack_start (GTK_BOX (ret), frame, FALSE, FALSE, 0);

    // label
    label = gtk_label_new (_ ("Destination folder"));
    gtk_table_attach (GTK_TABLE (table), label, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    // folder chooser button
    folderChooser = gtk_file_chooser_button_new (_ ("Select a folder"), GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER);
    gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (folderChooser), dftPath);
    g_signal_connect (G_OBJECT (folderChooser) , "selection_changed" , G_CALLBACK (record_path_changed) , NULL);
    gtk_table_attach (GTK_TABLE (table), folderChooser, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);

    // isAlwaysRecording functionality checkbox
    GtkWidget *enableIsAlwaysRecording = NULL;
    gboolean isAlwaysRecording = FALSE;

    isAlwaysRecording = dbus_get_is_always_recording();
    enableIsAlwaysRecording = gtk_check_button_new_with_mnemonic(_("_Always recording"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enableIsAlwaysRecording), isAlwaysRecording);
    g_signal_connect(G_OBJECT(enableIsAlwaysRecording), "clicked", active_is_always_recording, NULL);
    gtk_table_attach(GTK_TABLE(table), enableIsAlwaysRecording, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 5);
    gtk_widget_show(GTK_WIDGET(enableIsAlwaysRecording));

    /*
    gint value = dbus_get_echo_cancel_tail_length();
    echoTailLength = gtk_hscale_new_with_range(100, 500, 5);
    gtk_range_set_value(GTK_RANGE(echoTailLength), (gdouble)value);
    gtk_table_attach(GTK_TABLE(table), echoTailLength, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    g_signal_connect(G_OBJECT(echoTailLength), "value-changed", G_CALLBACK(echo_tail_length_changed), NULL);

    value = dbus_get_echo_cancel_delay();
    echoDelay = gtk_hscale_new_with_range(0, 500, 5);
    gtk_range_set_value(GTK_RANGE(echoDelay), (gdouble)value);
    gtk_table_attach(GTK_TABLE(table), echoDelay, 0, 1, 4, 5, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
    g_signal_connect(G_OBJECT(echoDelay), "value-changed", G_CALLBACK(echo_delay_changed), NULL);
    */

    gtk_widget_show_all (ret);

    return ret;
}
