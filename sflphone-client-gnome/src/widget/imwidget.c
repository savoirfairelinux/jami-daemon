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
#include <gdk/gdkkeysyms.h>

#define WEBKIT_DIR "file://" DATA_DIR "/webkit/"

	void
im_widget_add_message (callable_obj_t *call, const gchar *message)
{
	/* use the widget for this specific call, if exists */
	if (!call){
		error ("The call passed as a parameter does not seem to be valid");
	}
	else {

		IMWidget *im = IM_WIDGET (call->_im_widget);

		if (im) {

			/* Update the informations about the call in the chat window */
			im_widget_add_call_header (call);

			/* Prepare and execute the Javascript code */
			gchar *script = g_strdup_printf("add_message('%s', '%s', '%s', '%s');", message, call->_peer_name, call->_peer_number, call->_peer_info);
			webkit_web_view_execute_script (WEBKIT_WEB_VIEW(im->web_view), script);

			/* Cleanup */
			g_free(script);

		}

		else {

			/* If the chat window is not opened when we receive an incoming message, create the im widget first, 
			   then call the javascript to display the message */

			im = im_widget_new ();
			im_window_add (im);
			im->call = call;
			call->_im_widget = im;

			/* Prepare and execute the Javascript code */
			gchar *script = g_strdup_printf("add_message('%s', '%s', '%s', '%s');", message, call->_peer_name, call->_peer_number, call->_peer_info);
			webkit_web_view_execute_script (WEBKIT_WEB_VIEW(im->web_view), script);

			/* Cleanup */
			g_free(script);
		}
	}
}

void
im_widget_add_call_header (callable_obj_t *call) {

	IMWidget *im = IM_WIDGET (call->_im_widget);
	gchar *script = g_strdup_printf("add_call_info_header('%s', '%s', '%s');", call->_peer_name, call->_peer_number, call->_peer_info);
	webkit_web_view_execute_script (WEBKIT_WEB_VIEW(im->web_view), script);

	/* Cleanup */
	g_free(script);
}

static gboolean
web_view_nav_requested_cb(
		WebKitWebView             *web_view,
		WebKitWebFrame            *frame,
		WebKitNetworkRequest      *request,
		WebKitWebNavigationAction *navigation_action,
		WebKitWebPolicyDecision   *policy_decision,
		gpointer                   user_data)
{
	const gchar *uri = webkit_network_request_get_uri(request);

	/* Always allow files we are serving ourselves. */
	if (!strncmp(uri, WEBKIT_DIR, sizeof(WEBKIT_DIR) - 1)) {
		webkit_web_policy_decision_use(policy_decision);
	} else {
		printf("FIXME(jonas) open URL in browser: %s\n", uri);
		webkit_web_policy_decision_ignore(policy_decision);
	}
	return TRUE;
}

	static gboolean 
on_Textview_changed (GtkWidget *widget, GdkEventKey *event, gpointer user_data)
{

	GtkTextIter start, end;
	/* Get all the text in the buffer */
	IMWidget *im =  user_data;
	GtkTextBuffer *buffer =  gtk_text_view_get_buffer (GTK_TEXT_VIEW (im->textarea));

	if (event->type == GDK_KEY_PRESS){

		switch (event->keyval)
		{
			case GDK_Return:

				if (gtk_text_buffer_get_char_count (buffer) != 0 )
				{
					gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
					gchar *message = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

					/* Display our own message in the chat window */
					im_widget_add_message (im->call, message);

					/* Send the message to the peer */
					dbus_send_text_message (im->call->_callID, message);

					/* Empty the buffer */
					gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &start, &end);	

				}
				return TRUE;
		}
	}

	return FALSE;
}



	static void
im_widget_class_init(IMWidgetClass *klass)
{
}

	static void
im_widget_init (IMWidget *im)
{

	im->textarea = gtk_text_view_new ();
	im->web_view = webkit_web_view_new();
	GtkWidget *textscrollwin = gtk_scrolled_window_new (NULL, NULL);
	GtkWidget *webscrollwin = gtk_scrolled_window_new (NULL, NULL);

	/* A bar with the entry text and the button to send the message */
	GtkWidget *hbox = gtk_hbox_new (FALSE, 10);
	gtk_text_view_set_editable(GTK_TEXT_VIEW(im->textarea), TRUE);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(textscrollwin), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
	gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW(webscrollwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_widget_set_size_request(GTK_WIDGET (textscrollwin), -1, 20);
	gtk_widget_set_size_request(GTK_WIDGET (im->textarea), -1, 20);

	gtk_container_add (GTK_CONTAINER (textscrollwin), im->textarea);
	gtk_container_add (GTK_CONTAINER (webscrollwin), im->web_view);
	gtk_container_add (GTK_CONTAINER (hbox), textscrollwin);
	gtk_box_pack_start (GTK_BOX(im), webscrollwin, TRUE, TRUE, 5);
	gtk_box_pack_end (GTK_BOX(im), hbox, FALSE, FALSE, 2);
	g_signal_connect (im->web_view, "navigation-policy-decision-requested", G_CALLBACK (web_view_nav_requested_cb), NULL);
	g_signal_connect(im->textarea, "key-press-event", G_CALLBACK (on_Textview_changed), im);
	g_signal_connect (G_OBJECT (webscrollwin), "destroy", G_CALLBACK (gtk_main_quit), NULL);

	im->web_frame = webkit_web_view_get_main_frame(WEBKIT_WEB_VIEW(im->web_view));
	im->js_context = webkit_web_frame_get_global_context(im->web_frame);
	im->js_global = JSContextGetGlobalObject(im->js_context);
	webkit_web_view_load_uri (WEBKIT_WEB_VIEW(im->web_view), "file://" DATA_DIR "/webkit/im/im.html");
}

	GtkWidget *
im_widget_new()
{
	return GTK_WIDGET (g_object_new (IM_WIDGET_TYPE, NULL));
}

	GType
im_widget_get_type(void)
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
				GTK_TYPE_VBOX,
				"IMWidget",
				&im_widget_info,
				0);
	}

	return im_widget_type;
}

	void 
im_widget_display (callable_obj_t **call) 
{

	/* Work with a copy of the object */
	callable_obj_t *tmp = *call;

	/* Use the widget for this specific call, if exists */
	IMWidget *im = IM_WIDGET (tmp->_im_widget);

	if (!im) {
		g_print ("creating the im widget for this call\n");
		/* Create the im object */
		im = im_widget_new ();
		tmp->_im_widget = im;
		/* Update the call */
		*call = tmp;	
		im->call = *call;

		/* Add it to the main instant messaging window */
		gchar *label = get_peer_information (tmp);
		im_window_add (im, label);
	}
	else {
		g_print ("im widget exists for this call\n");
		im_window_show ();
	}

}
