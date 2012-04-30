/*
 *  Copyright (C) 2004-2012 Savoir-Faire Linux Inc.
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

#ifndef BOX_H_
#define BOX_H_

#include <gtk/gtk.h>

/* Wrappers needed if using gtk2 */
#if !GTK_CHECK_VERSION(3, 0, 0)
GtkWidget*
gtk_box_new(GtkOrientation orientation, gint spacing);

GtkWidget*
gtk_button_box_new(GtkOrientation orientation);

void
gtk_widget_get_preferred_size(GtkWidget* widget, GtkRequisition *min_size, GtkRequisition *natural_size);

GdkPixbuf *
gtk_widget_render_icon_pixbuf(GtkWidget *widget, const gchar *stock_id, GtkIconSize size);

GtkWidget *
gtk_scale_new_with_range(GtkOrientation orientation, gdouble min, gdouble max,
                         gdouble step);

#endif

#endif // BOX_H_
