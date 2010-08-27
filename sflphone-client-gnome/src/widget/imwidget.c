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
#include <icons/icon_factory.h>
#include <JavaScriptCore/JavaScript.h>
#include <gdk/gdkkeysyms.h>

#define WEBKIT_DIR "file://" DATA_DIR "/webkit/"

	void
im_widget_add_message (callable_obj_t *call, const gchar *from, const gchar *message, gint level)
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

			/* Compute the date the message was sent */
			gchar *msgtime = im_widget_add_message_time ();

			/* Check for the message level */
			gchar *css_class = (level == MESSAGE_LEVEL_ERROR ) ? "error" : "";

			/* Prepare and execute the Javascript code */
			gchar *script = g_strdup_printf("add_message('%s', '%s', '%s', '%s');", message, from, css_class, msgtime);
			webkit_web_view_execute_script (WEBKIT_WEB_VIEW(im->web_view), script);

			/* Cleanup */
			g_free(script);

		}

	}
}

void
im_widget_add_call_header (callable_obj_t *call) {

	if (call) {
		IMWidget *im = IM_WIDGET (call->_im_widget);
		gchar *script = g_strdup_printf("add_call_info_header('%s', '%s');", call->_peer_name, call->_peer_number);
		webkit_web_view_execute_script (WEBKIT_WEB_VIEW(im->web_view), script);

		/* Cleanup */
		g_free(script);
	}
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
		webkit_web_policy_decision_use (policy_decision);
	} else {
		/* Running a system command to open the URL in the user's default browser */
		gchar *cmd = g_strdup_printf("x-www-browser %s", uri); 
		system (cmd);
		webkit_web_policy_decision_ignore (policy_decision);
		g_free (cmd);
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
					im_widget_add_message (im->call, "Me", message, MESSAGE_LEVEL_NORMAL);

					/* Send the message to the peer */
					im_widget_send_message (im->call, message);

					/* Empty the buffer */
					gtk_text_buffer_delete (GTK_TEXT_BUFFER (buffer), &start, &end);	

				}
				return TRUE;
		}
	}

	return FALSE;
}

	gchar* 
im_widget_add_message_time () 
{

	time_t now;
	unsigned char str[100];

	/* Compute the current time */
	(void) time (&now);
	struct tm* ptr;
	ptr = localtime (&now);

	/* Get the time of the message. Format: HH:MM::SS */
	strftime ((char *)str, 100, "%R", (const struct tm *)ptr);
	gchar *res = g_strdup (str);

	/* Return the new value */
	return res;
}

	void 
im_widget_send_message (callable_obj_t *call, const gchar *message)
{

	/* First check if the call is in CURRENT state, otherwise it could not be sent */
	if (call->_type == CALL && call->_state == CALL_STATE_CURRENT)		
	{
		/* Ship the message through D-Bus */
		dbus_send_text_message (call->_callID, message);
	}
	else {
		/* Display an error message */
		im_widget_add_message (call, "sflphoned", "Oups, something went wrong! Unable to send text messages outside a call.", MESSAGE_LEVEL_ERROR);	
	}
}


	static void
im_widget_class_init(IMWidgetClass *klass)
{
}

	static void
im_widget_init (IMWidget *im)
{

	/* A text view to enable users to enter text */
	im->textarea = gtk_text_view_new ();

	/* The webkit widget to display the message */
	im->web_view = webkit_web_view_new();
	GtkWidget *textscrollwin = gtk_scrolled_window_new (NULL, NULL);
	GtkWidget *webscrollwin = gtk_scrolled_window_new (NULL, NULL);
	im->info_bar = gtk_info_bar_new ();

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
	gtk_box_pack_start (GTK_BOX(im), im->info_bar, FALSE, FALSE, 2);
	gtk_box_pack_start (GTK_BOX(im), webscrollwin, TRUE, TRUE, 5);
	gtk_box_pack_end (GTK_BOX(im), hbox, FALSE, FALSE, 2);
	g_signal_connect (im->web_view, "navigation-policy-decision-requested", G_CALLBACK (web_view_nav_requested_cb), NULL);
	g_signal_connect(im->textarea, "key-press-event", G_CALLBACK (on_Textview_changed), im);
	g_signal_connect (G_OBJECT (webscrollwin), "destroy", G_CALLBACK (gtk_main_quit), NULL);

	im->web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW(im->web_view));
	im->js_context = webkit_web_frame_get_global_context (im->web_frame);
	im->js_global = JSContextGetGlobalObject (im->js_context);
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
	if (tmp) {
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
			im_widget_infobar (im, label);
			im_window_add (im, label);
		}
		else {
			g_print ("im widget exists for this call\n");
			im_window_show ();
		}
	}

}

	void
im_widget_infobar (IMWidget *im, gchar *label) {

	GtkWidget *infobar = im->info_bar;
	GtkWidget *content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (infobar));
	callable_obj_t *call = im->call;
	gchar *msg1 = g_strdup_printf ("Calling %s", label);
	GtkWidget *call_label = gtk_label_new (msg1);
	im->info_state = call_state_image_widget (call->_state);
	GtkWidget *logoUser = gtk_image_new_from_stock (GTK_STOCK_USER, GTK_ICON_SIZE_LARGE_TOOLBAR);
    
	gtk_container_add (GTK_CONTAINER (content_area), logoUser);
	gtk_container_add (GTK_CONTAINER (content_area), call_label);
	gtk_container_add (GTK_CONTAINER (content_area), im->info_state);

    /* show an info message */
    gtk_info_bar_set_message_type (GTK_INFO_BAR (infobar),
                               GTK_MESSAGE_INFO);
    gtk_widget_show (infobar);

	free (msg1);

}

	GtkWidget*
call_state_image_widget (call_state_t state) {

	GtkWidget *image;

	switch (state) {
		case CALL_STATE_CURRENT:
			image = gtk_image_new_from_stock (GTK_STOCK_IM, GTK_ICON_SIZE_LARGE_TOOLBAR);
			break;
		default:
			image = gtk_image_new_from_stock (GTK_STOCK_FAIL, GTK_ICON_SIZE_LARGE_TOOLBAR);
			break;

	}
	return image;
}

	void
im_widget_update_state (IMWidget *im, gboolean active) 
{
	g_print ("asd;vmsn;bvs\n");

	GtkWidget *content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (im->info_bar));
	gtk_widget_set_sensitive (im->info_state, FALSE);
	gtk_widget_set_tooltip_text (im->info_state, "Call has terminated");
	if (active) {
		gtk_widget_set_sensitive (im->info_state, TRUE); 
		gtk_info_bar_set_message_type (GTK_INFO_BAR (im->info_bar),
                               GTK_MESSAGE_INFO);
	}
	else {
 		gtk_widget_set_sensitive (im->info_state, FALSE);
		gtk_info_bar_set_message_type (GTK_INFO_BAR (im->info_bar),
                               GTK_MESSAGE_WARNING);
	}
}



