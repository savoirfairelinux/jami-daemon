/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Pierre-Luc Bacon <pierre-luc.bacon@savoirfairelinux.com>
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

#include <config.h>
#include <gtk/gtk.h>
#include <eel-gconf-extensions.h>
#include <sflphone_const.h>

#include <imwindow.h>

/** Local variables */
GtkWidget *im_window = NULL;
GtkWidget *im_vbox = NULL;

static gboolean window_configure_cb (GtkWidget *win, GdkEventConfigure *event) {
	int pos_x, pos_y;

	eel_gconf_set_integer (CONF_IM_WINDOW_WIDTH, event->width);
	eel_gconf_set_integer (CONF_IM_WINDOW_HEIGHT, event->height);

	gtk_window_get_position (GTK_WINDOW (im_window_get()), &pos_x, &pos_y);
	eel_gconf_set_integer (CONF_IM_WINDOW_POSITION_X, pos_x);
	eel_gconf_set_integer (CONF_IM_WINDOW_POSITION_Y, pos_y);

	return FALSE;
}

/**
 * Minimize the main window.
 */
static gboolean
on_delete(GtkWidget * widget UNUSED, gpointer data UNUSED)
{
	gtk_widget_hide(GTK_WIDGET(im_window_get()));
	// gtk_widget_destroy (GTK_WIDGET(im_window_get()));
	return TRUE;
}

static void
im_window_init()
{
	const char *window_title = "SFLphone VoIP Client - Instant Messaging Module";
	gchar *path;
	GError *error = NULL;
	gboolean ret;
	int width, height, position_x, position_y;

	// Get configuration stored in gconf
	width = eel_gconf_get_integer(CONF_IM_WINDOW_WIDTH);
	if (width <= 0)
		width = 400;
	height = eel_gconf_get_integer(CONF_IM_WINDOW_HEIGHT);
	if (height <= 0)
		height = 500;
	position_x = eel_gconf_get_integer(CONF_IM_WINDOW_POSITION_X);
	position_y = eel_gconf_get_integer(CONF_IM_WINDOW_POSITION_Y);

	im_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_container_set_border_width(GTK_CONTAINER(im_window), 0);
	gtk_window_set_title(GTK_WINDOW(im_window), window_title);
	gtk_window_set_default_size(GTK_WINDOW (im_window), width, height);
	gtk_window_set_default_icon_from_file(LOGO, NULL);
	gtk_window_set_position(GTK_WINDOW(im_window), GTK_WIN_POS_MOUSE);

	gtk_widget_set_name(im_window, "imwindow");

	g_signal_connect (G_OBJECT (im_window), "delete-event",
		G_CALLBACK (on_delete), NULL);

	g_signal_connect_object (G_OBJECT (im_window), "configure-event",
		G_CALLBACK (window_configure_cb), NULL, 0);

	im_vbox = gtk_vbox_new (FALSE /*homogeneous*/, 0 /*spacing*/);

	gtk_container_add (GTK_CONTAINER (im_window), im_vbox);

	/* make sure that everything is visible */
	gtk_widget_show_all (im_window);

	// Restore position according to the configuration stored in gconf
	gtk_window_move (GTK_WINDOW (im_window), position_x, position_y);
}

GtkWidget *
im_window_get()
{
	if (im_window == NULL)
		im_window_init();
	return im_window;
}

void
im_window_add (GtkWidget *widget)
{
	if (im_window_get()) {
		gtk_box_pack_start (GTK_BOX (im_vbox), widget, TRUE /*expand*/,
			TRUE /*fill*/, 0 /*padding*/);
		gtk_widget_show_all (im_window);
	}
}

void
im_window_remove(GtkWidget *widget)
{
	// TODO(jonas) remove widget from the window
}
