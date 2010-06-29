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

#include "imwidget.h"
#include <JavaScriptCore/JavaScript.h>


static void im_widget_init(IMWidget *im);
static void im_widget_class_init(IMWidgetClass *klass);

GType
im_widget_get_type (void)
{
	static GType im_widget_type = 0;

	if (!im_widget_type) {
		static const GTypeInfo im_widget_info = {
			sizeof (IMWidgetClass),
			NULL, /* base_init */
			NULL, /* base_finalize */
			(GClassInitFunc) im_widget_class_init,
			NULL, /* class_finalize */
			NULL, /* class_data */
			sizeof (IMWidget),
			0,
			(GInstanceInitFunc) im_widget_init,
			NULL  /* value_table */
		};

		im_widget_type = g_type_register_static(
			WEBKIT_TYPE_WEB_VIEW,
			"IMWidget",
			&im_widget_info,
			0);
	}

	return im_widget_type;
}

static void
im_widget_class_init (IMWidgetClass *klass)
{
}

static void
im_widget_init (IMWidget *im)
{
	/* Load our initial webpage on startup */
	webkit_web_view_open(WEBKIT_WEB_VIEW(im), "file://" DATA_DIR "/webkit/im.html");

	/* Instantiate our local webkit related variables */
	im->web_frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(im));
	im->js_context = webkit_web_frame_get_global_context(im->web_frame);
	im->js_global = JSContextGetGlobalObject(im->js_context);
}

GtkWidget *
im_widget_new()
{
	return GTK_WIDGET(g_object_new(IM_WIDGET_TYPE, NULL));
}
