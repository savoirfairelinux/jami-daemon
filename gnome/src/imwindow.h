/*
 *  Copyright (C) 2010 Savoir-Faire Linux Inc.
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

#ifndef __IMWINDOW_H__
#define __IMWINDOW_H__

#include <gtk/gtk.h>
#include <gtk/gtk.h>

#include <widget/imwidget.h>

#define IM_WINDOW_WIDTH 280
#define IM_WINDOW_HEIGHT 320

/** @file imwindow.h
  * @brief The IM window of the client.
  */

/*!	@function
@abstract	Add IM widget to the IM window
 */
void im_window_add (GtkWidget *widget);

/*! @function
 @abstract	Remove IM widget from the IM window
 */
void im_window_remove_tab (GtkWidget *widget);

void im_window_show ();

/**
 * Return wether the instant messaging window have been created or not
 */
gboolean im_window_is_active (void);

/**
 * Return wether the instant messaging window is visible
 */
gboolean im_window_is_visible (void);

/**
 * Return the number of tabs already open in instant messaging window
 */
gint im_window_get_nb_tabs (void);

/*! @function
@abstract	Add a new tab in the notebook. Each tab is an IM Widget
@param		The IM widget
*/
void im_window_add_tab (GtkWidget *widget);

/*! @function
@abstract	Decide whether or not the notebook should display its tab. Display the tabs only if more than one tab is opened.
*/
void im_window_hide_show_tabs ();

/*! @function
@abstract Select the specified tab as current in instant messaging window
@param The tab to be set as current
*/
void im_window_show_tab (GtkWidget *widget);

#endif
