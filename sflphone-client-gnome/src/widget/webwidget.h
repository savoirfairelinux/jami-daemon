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


#ifndef __WEB_WIDGET_H__
#define __WEB_WIDGET_H__

#include <gtk/gtk.h>
#include <webkit/webkit.h>

G_BEGIN_DECLS

#define WEB_WIDGET_TYPE             (im_widget_get_type())
#define WEB_WIDGET(obj)             (G_TYPE_CHECK_INSTANCE_CAST((obj), WEB_WIDGET_TYPE, WebWidget))
#define IM_WIDGET_CLASS(vtable)    (G_TYPE_CHECK_CLASS_CAST((vtable), IM_WIDGET_TYPE, WebWidgetClass))
#define IS_IM_WIDGET(obj)          (G_TYPE_CHECK_INSTANCE_TYPE((obj), IM_WIDGET_TYPE))
#define IS_IM_WIDGET_CLASS(vtable) (G_TYPE_CHECK_CLASS_TYPE((vtable), IM_WIDGET_TYPE))
#define IM_WIDGET_GET_CLASS(inst)  (G_TYPE_INSTANCE_GET_CLASS((inst), IM_WIDGET_TYPE, WebWidgetClass))

typedef struct _WebWidget      WebWidget;
typedef struct _WebWidgetClass WebWidgetClass;

struct _WebWidget {
	WebKitWebView parent_instance;

	/* Private */
	WebKitWebFrame *web_frame;      // Our web frame
	JSGlobalContextRef js_context;  // The frame's global JS context
	JSObjectRef js_global;          // The frame's global context JS object
};

struct _WebWidgetClass {
	WebKitWebViewClass parent_class;
};


GType         im_widget_get_type         (void) G_GNUC_CONST;
GtkWidget    *im_widget_new              (void);

G_END_DECLS

#endif  /* __IM_WIDGET_H__ */
