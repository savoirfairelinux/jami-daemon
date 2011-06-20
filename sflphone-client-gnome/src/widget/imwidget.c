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

#include <imwindow.h>
#include "imwidget.h"
#include <icons/icon_factory.h>
#include <contacts/calltab.h>
#include <contacts/conferencelist.h>
#include <JavaScriptCore/JavaScript.h>
#include <gdk/gdkkeysyms.h>

#define WEBKIT_DIR "file://" DATA_DIR "/webkit/"

static void
on_frame_loading_done (GObject *gobject UNUSED, GParamSpec *pspec UNUSED, gpointer user_data)
{
    IMWidget *im = IM_WIDGET (user_data);
    callable_obj_t *call;
    conference_obj_t *conf;

    if (im->first_message && im->first_message_from) {
        switch (webkit_web_frame_get_load_status (WEBKIT_WEB_FRAME (im->web_frame))) {
            case WEBKIT_LOAD_PROVISIONAL:
            case WEBKIT_LOAD_COMMITTED:
                break;
            case WEBKIT_LOAD_FINISHED:
                call = calllist_get_call (current_calls, im->call_id);
                conf = conferencelist_get (current_calls, im->call_id);

                if (call)
                    im_widget_add_message (im, im->first_message_from, im->first_message, 0);

                if (conf)
                    im_widget_add_message (im, im->first_message_from, im->first_message, 0);

                g_free (im->first_message);
                g_free (im->first_message_from);
                im->first_message = NULL;
                im->first_message_from = NULL;
                DEBUG ("InstantMessaging: JavaScrip loading frame finished");
                break;
            case WEBKIT_LOAD_FIRST_VISUALLY_NON_EMPTY_LAYOUT:
                // case WEBKIT_LOAD_FAILED: // only available in webkit-1.0-2
                break;
	    case WEBKIT_LOAD_FAILED:
		DEBUG("InstantMessaging: Webkit load failed");
    		break;    
	    default:
		ERROR("InstantMessaging: Error: Not a valid case in switch");
	        break;
	}
    }

}

gchar *
escape_single_quotes (const gchar *message)
{
    gchar **ptr_token;
    gchar *string = "";

    DEBUG ("InstantMessaging: message: %s", message);

    if ( (ptr_token = g_strsplit (message, "'", 0))) {
        string = g_strjoinv ("\\'", ptr_token);
    }

    return string;
}

void
im_widget_add_message (IMWidget *im, const gchar *from, const gchar *message, gint level)
{

    if (im) {

        /* Compute the date the message was sent */
        gchar *msgtime = im_widget_add_message_time ();

        /* Check for the message level */
        gchar *css_class = (level == MESSAGE_LEVEL_ERROR) ? "error" : "";

        gchar *message_escaped = escape_single_quotes (message);

        /* Prepare and execute the Javascript code */
        gchar *script = g_strdup_printf ("add_message('%s', '%s', '%s', '%s');", message_escaped, from, css_class, msgtime);
        webkit_web_view_execute_script (WEBKIT_WEB_VIEW (im->web_view), script);

        /* Mark it as used */
        im->containText = TRUE;

        /* Cleanup */
        g_free (script);
        g_free (message_escaped);

    }
}

static gboolean
web_view_nav_requested_cb (
    WebKitWebView             *web_view UNUSED,
    WebKitWebFrame            *frame UNUSED,
    WebKitNetworkRequest      *request,
    WebKitWebNavigationAction *navigation_action UNUSED,
    WebKitWebPolicyDecision   *policy_decision,
    gpointer                   user_data UNUSED)
{
    const gchar *uri = webkit_network_request_get_uri (request);

    /* Always allow files we are serving ourselves. */
    if (!strncmp (uri, WEBKIT_DIR, sizeof (WEBKIT_DIR) - 1)) {
        webkit_web_policy_decision_use (policy_decision);
    } else {
        /* Running a system command to open the URL in the user's default browser */
        gchar *cmd = g_strdup_printf ("x-www-browser %s", uri);

        if (system (cmd) == -1)
            ERROR ("InstantMessaging: Error: executing command %s", cmd);

        webkit_web_policy_decision_ignore (policy_decision);
        g_free (cmd);
    }

    return TRUE;
}

static gboolean
on_Textview_changed (GtkWidget *widget UNUSED, GdkEventKey *event, gpointer user_data)
{

    GtkTextIter start, end;
    /* Get all the text in the buffer */
    IMWidget *im =  user_data;

    GtkTextBuffer *buffer =  gtk_text_view_get_buffer (GTK_TEXT_VIEW (im->textarea));

    /* Catch the keyboard events */
    if (event->type == GDK_KEY_PRESS) {

        switch (event->keyval) {
            case GDK_Return:
            case GDK_KP_Enter:

                /* We want to send the message on pressing ENTER */
                if (gtk_text_buffer_get_char_count (buffer) != 0) {
                    /* Fetch the string text */
                    gtk_text_buffer_get_bounds (GTK_TEXT_BUFFER (buffer), &start, &end);
                    gchar *message = gtk_text_buffer_get_text (buffer, &start, &end, FALSE);

                    /* Display our own message in the chat window */
                    im_widget_add_message (im, "Me", message, MESSAGE_LEVEL_NORMAL);

                    /* Send the message to the peer */
                    im_widget_send_message (im->call_id, message);

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
    strftime ( (char *) str, 100, "%R", (const struct tm *) ptr);
    gchar *res = g_strdup ( (gchar *) str);

    /* Return the new value */
    return res;
}

void
im_widget_send_message (gchar *id, const gchar *message)
{

    callable_obj_t *im_widget_call = calllist_get_call (current_calls, id);
    conference_obj_t *im_widget_conf = conferencelist_get (current_calls, id);

    /* If the call has been hungup, it is not anymore in the current_calls calltab */
    if (!im_widget_call) {
        /* So try the history tab */
        im_widget_call = calllist_get_call (history, id);
    }

    if (im_widget_conf) {
        dbus_send_text_message (id, message);
    }
    /* First check if the call is in CURRENT state, otherwise it could not be sent */
    else if (im_widget_call) {
        if (im_widget_call->_type == CALL && (im_widget_call->_state == CALL_STATE_CURRENT ||
                                              im_widget_call->_state == CALL_STATE_HOLD ||
                                              im_widget_call->_state == CALL_STATE_RECORD)) {
            /* Ship the message through D-Bus */
            dbus_send_text_message (id, message);
        } else {
            /* Display an error message */
            im_widget_add_message (IM_WIDGET (im_widget_call->_im_widget), "sflphoned", "Oups, something went wrong! Unable to send text messages outside a call.", MESSAGE_LEVEL_ERROR);
        }
    }
}


static void
im_widget_class_init (IMWidgetClass *klass UNUSED)
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
    gtk_text_view_set_editable (GTK_TEXT_VIEW (im->textarea), TRUE);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (textscrollwin), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (webscrollwin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request (GTK_WIDGET (textscrollwin), -1, 20);
    gtk_widget_set_size_request (GTK_WIDGET (im->textarea), -1, 20);
    gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (im->textarea), GTK_WRAP_CHAR);
    // gtk_container_set_resize_mode(GTK_CONTAINER(im->textarea), GTK_RESIZE_PARENT);

    gtk_container_add (GTK_CONTAINER (textscrollwin), im->textarea);
    gtk_container_add (GTK_CONTAINER (webscrollwin), im->web_view);
    gtk_container_add (GTK_CONTAINER (hbox), textscrollwin);
    gtk_box_pack_start (GTK_BOX (im), im->info_bar, FALSE, FALSE, 2);
    gtk_box_pack_start (GTK_BOX (im), webscrollwin, TRUE, TRUE, 5);
    gtk_box_pack_end (GTK_BOX (im), hbox, FALSE, FALSE, 2);
    g_signal_connect (im->web_view, "navigation-policy-decision-requested", G_CALLBACK (web_view_nav_requested_cb), NULL);
    g_signal_connect (im->textarea, "key-press-event", G_CALLBACK (on_Textview_changed), im);

    im->web_frame = webkit_web_view_get_main_frame (WEBKIT_WEB_VIEW (im->web_view));
    im->js_context = webkit_web_frame_get_global_context (im->web_frame);
    im->js_global = JSContextGetGlobalObject (im->js_context);
    webkit_web_view_load_uri (WEBKIT_WEB_VIEW (im->web_view), "file://" DATA_DIR "/webkit/im/im.html");

    im->containText = FALSE;

    g_signal_connect (G_OBJECT (im->web_frame), "notify", G_CALLBACK (on_frame_loading_done), im);
}

GtkWidget *
im_widget_new()
{
    return GTK_WIDGET (g_object_new (IM_WIDGET_TYPE, NULL));
}

GtkWidget *
im_widget_new_with_first_message (const gchar *message UNUSED)
{
    return GTK_WIDGET (g_object_new (IM_WIDGET_TYPE, NULL));
    // return GTK_WIDGET (g_object_new (IM_WIDGET_TYPE, "first_message", message, NULL));
}


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

        im_widget_type = g_type_register_static (
                             GTK_TYPE_VBOX,
                             "IMWidget",
                             &im_widget_info,
                             0);
    }

    return im_widget_type;
}

gboolean
im_widget_display (IMWidget **im, const gchar *message, const gchar *id, const gchar *from)
{

    /* Work with a copy of the object */
    // callable_obj_t *tmp = *call;

    /* Use the widget for this specific call, if exists */
    // if (tmp) {
    IMWidget *imwidget = *im;// = IM_WIDGET (tmp->_im_widget);

    if (!imwidget) {
        DEBUG ("creating the im widget for this call\n");

        /* Create the im object, first message must be created asynchronously */
        if (message)
            imwidget = IM_WIDGET (im_widget_new ());
        else
            imwidget = IM_WIDGET (im_widget_new_with_first_message (message));

        /* Keep a reference on this object in the call struct */
        // tmp->_im_widget = im;
        // *call = tmp;

        /* Update the widget with some useful call information: ie the call ID */
        imwidget->call_id = (gchar *)id;

        /* Create the GtkInfoBar, used to display call information, and status of the IM widget */
        im_widget_infobar (imwidget);

        /* Add it to the main instant messaging window */
        im_window_add (GTK_WIDGET (imwidget));

        /* Update the first message to appears at widget creation*/
        if (message)
            imwidget->first_message = g_strdup (message);

        if (from)
            imwidget->first_message_from = g_strdup (from);

        *im = imwidget;

        return FALSE;
    } else {
        DEBUG ("im widget exists for this call\n");
        im_window_show ();

        return TRUE;
    }

    // }

    // return FALSE;
}

void
im_widget_infobar (IMWidget *im)
{

    /* Fetch the GTKInfoBar of this very IM Widget */
    GtkWidget *infobar = im->info_bar;
    GtkWidget *content_area = gtk_info_bar_get_content_area (GTK_INFO_BAR (infobar));

    /* Fetch call/conference information */
    callable_obj_t *im_widget_call = calllist_get_call (current_calls, im->call_id);
    conference_obj_t *im_widget_conf = conferencelist_get (current_calls, im->call_id);

    /* Create the label widgets with the call information saved in the IM Widget struct */
    gchar *msg1;

    if (im_widget_call)
        msg1 = g_strdup_printf ("Calling %s  %s", im_widget_call->_peer_number, im_widget_call->_peer_name);
    else if (im_widget_conf)
        msg1 = g_strdup_printf ("Conferencing"); // im_widget_conf->_confID);
    else
        msg1 = g_strdup ("");

    GtkWidget *call_label = gtk_label_new (msg1);

    if (im_widget_call)
        im->info_state = call_state_image_widget (im_widget_call->_state);

    if (im_widget_conf)
        im->info_state = conf_state_image_widget (im_widget_conf->_state);

    /* Add a nice icon from our own icon factory */
    GtkWidget *logoUser = gtk_image_new_from_stock (GTK_STOCK_USER, GTK_ICON_SIZE_LARGE_TOOLBAR);

    /* Pack it all */
    gtk_container_add (GTK_CONTAINER (content_area), logoUser);
    gtk_container_add (GTK_CONTAINER (content_area), call_label);
    gtk_container_add (GTK_CONTAINER (content_area), im->info_state);

    /* Message level by default: INFO */
    gtk_info_bar_set_message_type (GTK_INFO_BAR (infobar), GTK_MESSAGE_INFO);

    /* Show the info bar */
    gtk_widget_show (infobar);

    /* Clean up */
    free (msg1);
}

GtkWidget*
call_state_image_widget (call_state_t state)
{

    GtkWidget *image;

    switch (state) {
        case CALL_STATE_CURRENT:
        case CALL_STATE_HOLD:
        case CALL_STATE_RECORD:
            image = gtk_image_new_from_stock (GTK_STOCK_IM, GTK_ICON_SIZE_LARGE_TOOLBAR);
            break;
        default:
            image = gtk_image_new_from_stock (GTK_STOCK_IM, GTK_ICON_SIZE_LARGE_TOOLBAR);
            break;

    }

    return image;
}

GtkWidget*
conf_state_image_widget (conference_state_t state)
{

    GtkWidget *image;

    switch (state) {
        case CONFERENCE_STATE_ACTIVE_ATACHED:
        case CONFERENCE_STATE_ACTIVE_DETACHED:
        case CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD:
        case CONFERENCE_STATE_ACTIVE_DETACHED_RECORD:
        case CONFERENCE_STATE_HOLD:
        case CONFERENCE_STATE_HOLD_RECORD:
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
    /* if active = true, it means that we are the call is in current state, so sflphone can send text messages */
    if (active) {
        gtk_widget_set_sensitive (im->info_state, TRUE);
        gtk_info_bar_set_message_type (GTK_INFO_BAR (im->info_bar),
                                       GTK_MESSAGE_INFO);
    }
    /* if active = false, the call is over, we can't send text messages anymore */
    else {
        if (im) {
            gtk_widget_set_sensitive (im->info_state, FALSE);
            gtk_info_bar_set_message_type (GTK_INFO_BAR (im->info_bar),
                                           GTK_MESSAGE_WARNING);
            gtk_widget_set_tooltip_text (im->info_state, "Call has terminated");
        }
    }
}



