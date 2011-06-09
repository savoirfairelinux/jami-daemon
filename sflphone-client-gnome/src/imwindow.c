/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
#include <contacts/calltab.h>
#include <contacts/calltab.h>

/** Local variables */
GtkWidget *im_window = NULL;
GtkWidget *im_notebook = NULL;

static gboolean window_configure_cb (GtkWidget *wini UNUSED, GdkEventConfigure *event)
{
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
on_delete (GtkWidget * widget UNUSED, gpointer data UNUSED)
{
    /* Only hide the main window that contains all the instant messaging instances */
    gtk_widget_hide (GTK_WIDGET (im_window_get()));
    return TRUE;
}

static void
on_switch_page (GtkNotebook *notebook, GtkNotebookPage *page UNUSED, guint page_num, gpointer userdata UNUSED)
{
    guint index = gtk_notebook_get_current_page (GTK_NOTEBOOK (notebook));

    GtkWidget *tab = gtk_notebook_get_nth_page (GTK_NOTEBOOK (notebook), page_num);

    // show the current widget
    gtk_widget_grab_focus (tab);
    gtk_widget_show_now (tab);

    DEBUG ("InstantMessaging: switch to %i -  current = %i\n", page_num, index);
}

static void
im_window_init()
{
    const char *window_title = "SFLphone IM Client";
    int width, height, position_x, position_y;

    // Get configuration stored in gconf
    width = eel_gconf_get_integer (CONF_IM_WINDOW_WIDTH);

    if (width <= 0)
        width = 400;

    height = eel_gconf_get_integer (CONF_IM_WINDOW_HEIGHT);

    if (height <= 0)
        height = 500;

    position_x = eel_gconf_get_integer (CONF_IM_WINDOW_POSITION_X);
    position_y = eel_gconf_get_integer (CONF_IM_WINDOW_POSITION_Y);

    im_window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
    gtk_container_set_border_width (GTK_CONTAINER (im_window), 0);
    gtk_window_set_title (GTK_WINDOW (im_window), window_title);
    gtk_window_set_default_size (GTK_WINDOW (im_window), width, height);
    gtk_window_set_default_icon_from_file (LOGO, NULL);
    gtk_window_set_position (GTK_WINDOW (im_window), GTK_WIN_POS_MOUSE);

    gtk_widget_set_name (im_window, "imwindow");

    GtkWidget *im_vbox = gtk_vbox_new (FALSE /*homogeneous*/, 0 /*spacing*/);
    im_notebook = gtk_notebook_new ();

    gtk_container_add (GTK_CONTAINER (im_window), im_vbox);
    gtk_box_pack_start (GTK_BOX (im_vbox), im_notebook, TRUE, TRUE, 0);
    gtk_widget_show (im_notebook);

    g_signal_connect (G_OBJECT (im_window), "delete-event", G_CALLBACK (on_delete), NULL);
    g_signal_connect_object (G_OBJECT (im_window), "configure-event", G_CALLBACK (window_configure_cb), NULL, 0);
    g_signal_connect (G_OBJECT (im_notebook), "switch-page", G_CALLBACK (on_switch_page), NULL);

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


gboolean
im_window_is_active ()
{

    if (!im_window)
        return FALSE;
    else
        return gtk_window_is_active (GTK_WINDOW (im_window));
}

gboolean
im_window_is_visible ()
{
    return gtk_widget_get_visible (im_window);
}

void
im_window_show ()
{
    gtk_window_present (GTK_WINDOW (im_window_get ()));
}

void
im_window_add (GtkWidget *widget)
{
    if (im_window_get()) {
        /* Add the new tab to the notebook */
        im_window_add_tab (widget);

        /* Show it all */
        gtk_widget_show_all (im_window);
    } 
    else {
        ERROR ("InstantMessaging: Error: Could not create the main instant messaging window");
    }
}

gint
im_window_get_nb_tabs()
{
    if (im_notebook != NULL)
        return gtk_notebook_get_n_pages (GTK_NOTEBOOK(im_notebook));
    else
        return 0;
}

static void
close_tab_cb (GtkButton *button UNUSED, gpointer userdata)
{
    /* We want here to close the current tab */
    im_window_remove_tab (GTK_WIDGET (userdata));

    /* If no tabs are opened anymore, close the IM window */
    // gtk_widget_destroy (im_window);
}

void
im_window_add_tab (GtkWidget *widget)
{
    /* Cast the paramater */
    IMWidget *im = IM_WIDGET (widget);

    /* Fetch the call */
    callable_obj_t *im_widget_call = calllist_get (current_calls, im->call_id);
    conference_obj_t *im_widget_conf = conferencelist_get (im->call_id);

    /* A container to include the tab label and the close button */
    GtkWidget *tab_Container = gtk_hbox_new (FALSE, 3);
    GtkWidget *tab_Label;
    im->tab = tab_Container;

    if (im_widget_call)
        tab_Label = gtk_label_new (get_peer_information (im_widget_call));
    else if (im_widget_conf)
        tab_Label = gtk_label_new ("Conferencing");
    else
        tab_Label = gtk_label_new ("");

    GtkWidget *tab_CloseButton = gtk_button_new ();

    /* Pack it all */
    gtk_button_set_relief (GTK_BUTTON (tab_CloseButton), GTK_RELIEF_NONE);
    gtk_box_pack_start (GTK_BOX (tab_Container), tab_Label, TRUE, TRUE, 0);
    gtk_box_pack_start (GTK_BOX (tab_Container), tab_CloseButton, FALSE, FALSE, 0);
    gtk_container_add (GTK_CONTAINER (tab_CloseButton), gtk_image_new_from_stock (GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU));

    /* Connect a signal to the close button on each tab, to be able to close the tabs individually */
    g_signal_connect (tab_CloseButton, "clicked", G_CALLBACK (close_tab_cb), widget);

    /* Show it */
    gtk_widget_show_all (im_notebook);
    gtk_widget_show_all (tab_Container);

    /* Add the page to the notebook */
    guint tabIndex = gtk_notebook_append_page (GTK_NOTEBOOK (im_notebook), widget, tab_Container);

    /* TODO Switch to the newly opened tab. Still not working */
    DEBUG ("InstantMessaging: Switch to tab: %i", tabIndex);
    gtk_notebook_set_current_page (GTK_NOTEBOOK (im_notebook), -1);

    /* Decide whether or not displaying the tabs of the notebook */
    im_window_hide_show_tabs ();
}

void
im_window_hide_show_tabs ()
{
    /* If only one tab is open, do not display the tab, only the content */
    if (gtk_notebook_get_n_pages (GTK_NOTEBOOK (im_notebook)) == 1) {
        gtk_notebook_set_show_tabs (GTK_NOTEBOOK (im_notebook), FALSE);
    } else
        gtk_notebook_set_show_tabs (GTK_NOTEBOOK (im_notebook), TRUE);

}

void
im_window_show_tab (GtkWidget *widget)
{
    int pageIndex = gtk_notebook_page_num (GTK_NOTEBOOK (im_notebook), widget);

    if (pageIndex != -1)
        gtk_notebook_set_current_page (GTK_NOTEBOOK (im_notebook), pageIndex);
}

void
im_window_remove_tab (GtkWidget *widget)
{
    // Remove the widget from the window

    /* We want here to close the current tab */
    guint index = gtk_notebook_page_num (GTK_NOTEBOOK (im_notebook), GTK_WIDGET (widget));
    gtk_notebook_remove_page (GTK_NOTEBOOK (im_notebook), index);

    /* Need to do some memory clean up, so that we could re-open an Im widget for this call later. */
    IMWidget *im = IM_WIDGET (widget);
    callable_obj_t *call = calllist_get (current_calls, im->call_id);
    conference_obj_t *conf = conferencelist_get (im->call_id);

    if (call)
        call->_im_widget = NULL;

    if (conf)
        conf->_im_widget = NULL;

    /* Decide whether or not displaying the tabs of the notebook */
    im_window_hide_show_tabs ();
}
