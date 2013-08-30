/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#include "utils.h"
#include "sflphone_const.h"

void gnome_main_section_new_with_grid(const gchar *title, GtkWidget **frame, GtkWidget **grid)
{
    PangoAttrList *attrs = pango_attr_list_new();
    PangoAttribute *attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    attr->start_index = 0;
    attr->end_index = -1;
    pango_attr_list_insert(attrs, attr);

    *frame = gtk_frame_new(title);
    gtk_frame_set_shadow_type(GTK_FRAME(*frame), GTK_SHADOW_NONE);
    gtk_container_set_border_width(GTK_CONTAINER(*frame), 2);

    GtkWidget *label = gtk_frame_get_label_widget(GTK_FRAME(*frame));
    gtk_label_set_attributes(GTK_LABEL(label), attrs);
    pango_attr_list_unref(attrs);

    GtkWidget *align = gtk_alignment_new(0.08, 0.2, 0.1, 0.1);
    gtk_container_add(GTK_CONTAINER(*frame), align);

    *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(*grid), 2);
    gtk_grid_set_column_spacing(GTK_GRID(*grid), 2);
    gtk_widget_show(*grid);
    gtk_container_add(GTK_CONTAINER(align), *grid);
}


GtkWidget *gnome_main_section_new(const gchar * const title)
{
    PangoAttrList *attrs = pango_attr_list_new();
    PangoAttribute *attr = pango_attr_weight_new(PANGO_WEIGHT_BOLD);
    attr->start_index = 0;
    attr->end_index = -1;
    pango_attr_list_insert(attrs, attr);

    GtkWidget *frame = gtk_frame_new(title);
    gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_NONE);
    gtk_container_set_border_width(GTK_CONTAINER(frame), 2);

    GtkWidget *label = gtk_frame_get_label_widget(GTK_FRAME(frame));
    gtk_label_set_attributes(GTK_LABEL(label), attrs);
    pango_attr_list_unref(attrs);

    return frame;
}

GtkWidget *gnome_info_bar(gchar *message, GtkMessageType type)
{
    GtkWidget *info_bar = gtk_info_bar_new();
    gtk_widget_set_no_show_all(info_bar, TRUE);
    GtkWidget *message_label = gtk_label_new(NULL);
    gtk_widget_show(message_label);
    GtkWidget *content_area = gtk_info_bar_get_content_area(GTK_INFO_BAR(info_bar));
    gtk_container_add(GTK_CONTAINER(content_area), message_label);
    gtk_label_set_markup(GTK_LABEL(message_label), message);
    gtk_info_bar_set_message_type(GTK_INFO_BAR(info_bar), type);
    gtk_widget_show(info_bar);

    return info_bar;
}
