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

#include "dialpad.h"
#include "actions.h"
#include "calltab.h"

/**
 * button pressed event
 */

typedef struct
{
    const gchar *number;
    SFLPhoneClient *client;
} DialpadData;

static void
dialpad_pressed(G_GNUC_UNUSED GtkWidget * widget, DialpadData *data)
{
    gtk_widget_grab_focus(GTK_WIDGET(current_calls_tab->view));
    sflphone_keypad(0, data->number, data->client);
}

static void
dialpad_cleanup(G_GNUC_UNUSED GtkWidget * widget, DialpadData *data)
{
    g_free(data);
}

GtkWidget *
get_numpad_button(const gchar* number, const gchar * letters, SFLPhoneClient *client)
{
    GtkWidget *button = gtk_button_new();
    GtkWidget *label = gtk_label_new("1");
    gtk_label_set_single_line_mode(GTK_LABEL(label), FALSE);
    gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);
    gchar *markup = g_markup_printf_escaped("<big><b>%s</b></big>\n%s", number, letters);
    gtk_label_set_markup(GTK_LABEL(label), markup);
    gtk_container_add(GTK_CONTAINER(button), label);
    DialpadData * dialpad_data = g_new0(DialpadData, 1);
    dialpad_data->number = number;
    dialpad_data->client = client;
    g_signal_connect(G_OBJECT(button), "clicked",
                     G_CALLBACK(dialpad_pressed), dialpad_data);
    g_signal_connect(G_OBJECT(button), "destroy",
                     G_CALLBACK(dialpad_cleanup), dialpad_data);

    g_free(markup);
    return button;
}

GtkWidget *
create_dialpad(SFLPhoneClient *client)
{
    static const gchar * const key_strings[] = {
        "1", "",
        "2", "a b c",
        "3", "d e f",
        "4", "g h i",
        "5", "j k l",
        "6", "m n o",
        "7", "p q r s",
        "8", "t u v",
        "9", "w x y z",
        "*", "",
        "0", "",
        "#", ""
    };
    enum {ROWS = 4, COLS = 3};
    GtkWidget *grid = gtk_grid_new();
    g_object_set(G_OBJECT(grid), "column-homogeneous", TRUE, "row-homogeneous", TRUE, NULL);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 5);

    for (int row = 0, entry = 0; row != ROWS; ++row)
        for (int col = 0; col != COLS; ++col) {
            GtkWidget *button = get_numpad_button(key_strings[entry], key_strings[entry + 1], client);
            gtk_grid_attach(GTK_GRID(grid), button, col, row, 1, 1);
            entry += 2;
        }

    return grid;
}
