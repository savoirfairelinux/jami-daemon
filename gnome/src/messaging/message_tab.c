/*
 *  Copyright (C) 2012-2013 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>
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
#include "message_tab.h"

#include "../dbus/dbus.h"
#include <glib.h>
#include "../mainwindow.h"
#include <string.h>

static GtkWidget  *tab_box    = NULL ;
static GHashTable *tabs       = NULL ;
static GtkPaned   *paned      = NULL ;
static int        vpanes_s    = -1   ;
static int        skip_height = -3   ;


static GtkTextIter* start_link = NULL;
static GtkTextIter* end_link   = NULL;

/////////////////////HELPERS/////////////////////////


/*I really don't know why we need this, but without, it doesn't work*/
message_tab *
force_lookup(const gchar *id)
{
   GList *list = g_hash_table_get_keys(tabs);
   for (guint k=0;k<g_list_length(list);k++) {
      if (!strcmp(id,(gchar*)g_list_nth(list,k)->data)) {
         return g_hash_table_lookup(tabs,(const gchar*)g_list_nth(list,k)->data);
      }
   }
   return NULL;
}

void
disable_conference_calls(conference_obj_t *call)
{
    if (tabs) {
        guint size = g_slist_length(call->participant_list);
        for (guint i = 0; i < size;i++) {
               const gchar* id = (gchar*)g_slist_nth(call->participant_list,i)->data;
               message_tab *tab = force_lookup(id);
               if (tab)
                   gtk_widget_hide(tab->entry);
        }
    }
}



/////////////////////GETTERS/////////////////////////

GtkWidget *get_tab_box()
{
   if (!tab_box) {
      tab_box = gtk_notebook_new();
      gtk_notebook_set_scrollable(GTK_NOTEBOOK(tab_box),TRUE);
   }
   return tab_box;
}



/////////////////////SETTERS/////////////////////////

void
toggle_messaging()
{
    GtkWidget *box = get_tab_box();
    if (gtk_widget_get_visible(box))
        gtk_widget_show(box);
    else
        gtk_widget_hide(box);
}


void
show_messaging()
{
    gtk_widget_show(get_tab_box());
    if (vpanes_s > 0)
        gtk_paned_set_position(GTK_PANED(paned),vpanes_s);
}

void
set_message_tab_height(GtkPaned* _paned, int height)
{
   if ( skip_height >=0 || skip_height == -3 ) {
      paned    = _paned;
      vpanes_s = height;
   }
   skip_height++;
}

static void
append_message(message_tab* self, const gchar* name, const gchar* message)
{
    GtkTextIter current_end,new_end;
    gtk_text_buffer_get_end_iter( self->buffer, &current_end           );
    gtk_text_buffer_insert      ( self->buffer, &current_end, name, -1 );
    gtk_text_buffer_insert      ( self->buffer, &current_end, ": ", -1 );

    gtk_text_buffer_get_end_iter(self->buffer, &current_end);
    for (unsigned int i=0;i<strlen(name)+2;i++){
        if (!gtk_text_iter_backward_char(&current_end))
            break;
    }

    gtk_text_buffer_get_end_iter(self->buffer, &new_end);
    gtk_text_buffer_apply_tag_by_name(self->buffer, "b", &current_end, &new_end);

    gtk_text_buffer_insert      ( self->buffer, &new_end, message,    -1 );
    gtk_text_buffer_insert      ( self->buffer, &new_end, "\n"   ,    -1 );
    gtk_text_buffer_get_end_iter( self->buffer, &new_end                 );
    gtk_text_view_scroll_to_iter( self->view  , &new_end,FALSE,0,0,FALSE );

    start_link = NULL;
    end_link   = NULL;
}

//////////////////////SLOTS//////////////////////////

static void
on_enter(GtkEntry *entry, gpointer user_data)
{
    start_link = NULL;
    end_link   = NULL;
    message_tab *tab = (message_tab*)user_data;
    append_message(tab,(gchar*)"Me",gtk_entry_get_text(entry));
    if (tab->call)
        dbus_send_text_message(tab->call->_callID,gtk_entry_get_text(entry));
    else if (tab->conf)
        dbus_send_text_message(tab->conf->_confID,gtk_entry_get_text(entry));
    gtk_entry_set_text(entry,"");
}

static void
on_close(G_GNUC_UNUSED GtkWidget *button, gpointer data)
{
    message_tab *tab = (message_tab*)data;
    gtk_widget_destroy(tab->widget);
    if (tab->call)
      g_hash_table_remove(tabs,tab->call->_callID);
    else if (tab->conf)
       g_hash_table_remove(tabs,tab->conf->_confID);
}

static void
on_focus_in(G_GNUC_UNUSED GtkEntry *entry, G_GNUC_UNUSED gpointer user_data)
{
    main_window_pause_keygrabber(TRUE);
}

static void
on_focus_out(G_GNUC_UNUSED GtkEntry *entry, G_GNUC_UNUSED gpointer user_data)
{
    main_window_pause_keygrabber(FALSE);
}

static void
on_clicked(GtkTextBuffer *textbuffer, G_GNUC_UNUSED GtkTextIter *location, G_GNUC_UNUSED GtkTextMark *mark, SFLPhoneClient *client)
{
   if (start_link && end_link && gtk_text_iter_compare(start_link,location) <= 0 && gtk_text_iter_compare(location,end_link) <= 0) {
       gchar* text = gtk_text_buffer_get_text(textbuffer,start_link,end_link,FALSE);
       start_link = NULL;
       end_link = NULL;
       if (strlen(text)) {
           gchar* url_command = g_settings_get_string(client->settings, "messaging-url-command");
           if (!url_command || !strlen(url_command))
               url_command = g_strdup("xdg-open");

           const gchar* argv[] = {url_command, text, NULL};
           g_spawn_async(NULL,(gchar**)argv,NULL,G_SPAWN_SEARCH_PATH|G_SPAWN_STDOUT_TO_DEV_NULL|G_SPAWN_STDERR_TO_DEV_NULL,NULL,NULL,NULL,NULL);
           gtk_text_buffer_remove_all_tags(textbuffer,start_link,end_link );
           start_link = NULL;
           end_link   = NULL;
           g_free(url_command);
       }
   }
}

static void
on_cursor_motion(G_GNUC_UNUSED GtkTextView *view, GdkEvent *event, gpointer data)
{
   /* Convert mouse position into text iterators*/
   gint x,y;
   GtkTextIter cursor_pos,end_iter,end_match,start_match,end_match_b,start_match_b,start_real,end_real;
   gtk_text_buffer_get_end_iter          (((message_tab*) data)->buffer, &end_iter                                                              );
   gtk_text_view_window_to_buffer_coords (GTK_TEXT_VIEW(view),GTK_TEXT_WINDOW_TEXT,((GdkEventMotion*)event)->x,((GdkEventMotion*)event)->y,&x,&y);
   gtk_text_view_get_iter_at_location    (GTK_TEXT_VIEW(view),&cursor_pos,x,y                                                                   );
   gboolean ret = gtk_text_iter_backward_search(&cursor_pos," ",GTK_TEXT_SEARCH_TEXT_ONLY | GTK_TEXT_SEARCH_VISIBLE_ONLY,&start_match_b,&end_match_b,NULL);
   if ( ret ) {
        if (gtk_text_iter_forward_search(&cursor_pos," ",GTK_TEXT_SEARCH_TEXT_ONLY | GTK_TEXT_SEARCH_VISIBLE_ONLY,&start_match,&end_match,NULL)
           && gtk_text_iter_get_line(&end_match) == gtk_text_iter_get_line(&cursor_pos)) {
            start_real = end_match_b;
            end_real   = start_match;
        }
        else {
            gtk_text_iter_forward_visible_line(&cursor_pos);
            start_real = end_match_b;
            end_real   = cursor_pos ;
        }

        /*Get the word under cursor*/
        gchar* text = gtk_text_buffer_get_text(((message_tab*) data)->buffer,&start_real,&end_real,FALSE);

        /*Match the regex*/
        GError     *error          = NULL;
        gchar      *pattern_string = "^[a-z]*\\://[a-zA-Z0-9\\-\\.]+\\.[a-zA-Z]{2,3}(/\\S*)?$";
        GRegex     *regex          = g_regex_new( pattern_string, 0, 0, &error );
        GMatchInfo *match_info     = NULL;
        GdkWindow  *win            = gtk_text_view_get_window(GTK_TEXT_VIEW(view),GTK_TEXT_WINDOW_TEXT);

        g_regex_match( regex, text, 0, &match_info );
        if (g_match_info_matches( match_info )) {
            /*Is a link*/
            while( g_match_info_matches( match_info ) ) {
                  g_match_info_next( match_info, &error );
                  if (gtk_text_iter_get_buffer(&start_real) == ((message_tab*) data)->buffer && gtk_text_iter_get_buffer(&end_real) == ((message_tab*) data)->buffer) {
                      gtk_text_buffer_remove_all_tags(((message_tab*) data)->buffer,&start_real, &end_real);
                      gtk_text_buffer_apply_tag_by_name(((message_tab*) data)->buffer, "link", &start_real, &end_real);
                  }
            }
            GdkCursor *cur = gdk_cursor_new(GDK_HAND2);
            start_link     = gtk_text_iter_copy(&start_real);
            end_link       = gtk_text_iter_copy(&end_real);
            gdk_window_set_cursor(win,cur);
        }
        else {
            /*Is not a link, cleaning previous link*/
            GdkCursor *cur = gdk_cursor_new(GDK_XTERM);
            gdk_window_set_cursor(win,cur);
            if (start_link && end_link && gtk_text_iter_get_buffer(start_link) == ((message_tab*) data)->buffer && gtk_text_iter_get_buffer(end_link) == ((message_tab*) data)->buffer) {
               gtk_text_buffer_remove_all_tags(((message_tab*) data)->buffer,start_link,end_link );
                /*g_free(start_link);
                g_free(end_link);*/
               start_link = NULL;
               end_link   = NULL;
            }
        }
   }
}


/////////////////////MUTATORS////////////////////////

void
disable_messaging_tab(const gchar * id)
{
    message_tab *tab = NULL;
    if (tabs)
        tab = g_hash_table_lookup(tabs, id);
    if (tab != NULL)
        gtk_widget_hide(tab->entry);
    if (!g_list_length(gtk_container_get_children(GTK_CONTAINER(get_tab_box()))))
       gtk_widget_hide(get_tab_box());
}

static message_tab *
create_messaging_tab_common(const gchar* call_id, const gchar *label, SFLPhoneClient *client)
{
    show_messaging();
    /* Do not create a new tab if it already exist */
    message_tab *tab = NULL;
    if (tabs)
        tab = g_hash_table_lookup(tabs,call_id);
    if (tab) {
       return tab;
    }

    message_tab *self = g_new0(message_tab, 1);

    /* Create the main layout */
    GtkWidget *vbox            = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkTextBuffer *text_buffer = gtk_text_buffer_new(NULL);
    if (text_buffer) {
      gtk_text_buffer_create_tag(text_buffer, "b", "weight", PANGO_WEIGHT_BOLD,NULL);
      gtk_text_buffer_create_tag(text_buffer, "link", "foreground", "#0000FF","underline",PANGO_UNDERLINE_SINGLE,NULL);
    }

    /* Create the conversation history widget*/
    GtkWidget *history_hbox    = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2  );
    GtkWidget *h_left_spacer   = gtk_label_new ( ""                         );
    GtkWidget *h_right_spacer  = gtk_label_new ( ""                         );
    GtkWidget *scoll_area      = gtk_scrolled_window_new      ( NULL,NULL   );
    GtkWidget *text_box_widget = gtk_text_view_new_with_buffer( text_buffer );
    gtk_box_pack_start(GTK_BOX(history_hbox) , h_left_spacer  , FALSE , FALSE , 0);
    gtk_box_pack_start(GTK_BOX(history_hbox) , scoll_area     , TRUE  , TRUE  , 0);
    gtk_box_pack_start(GTK_BOX(history_hbox) , h_right_spacer , FALSE , FALSE , 0);

    gtk_text_view_set_editable ( GTK_TEXT_VIEW(text_box_widget),FALSE        );
    gtk_text_view_set_wrap_mode( GTK_TEXT_VIEW(text_box_widget),GTK_WRAP_CHAR);

    gtk_container_add(GTK_CONTAINER(scoll_area), text_box_widget);

    g_signal_connect(G_OBJECT(text_box_widget), "motion-notify-event" , G_CALLBACK(on_cursor_motion), self);
    g_signal_connect(G_OBJECT(text_buffer    ), "mark-set"            , G_CALLBACK(on_clicked      ), client);

    GtkWidget *line_edit    = gtk_entry_new (                               );
    GtkWidget *hbox         = gtk_box_new   ( GTK_ORIENTATION_HORIZONTAL, 1 );
    GtkWidget *left_spacer  = gtk_label_new ( ""                            );
    GtkWidget *right_spacer = gtk_label_new ( ""                            );
    gtk_box_pack_start(GTK_BOX(hbox) , left_spacer  , FALSE , FALSE , 0);
    gtk_box_pack_start(GTK_BOX(hbox) , line_edit    , TRUE  , TRUE  , 0);
    gtk_box_pack_start(GTK_BOX(hbox) , right_spacer , FALSE , FALSE , 0);

    g_signal_connect(G_OBJECT(line_edit), "activate"        , G_CALLBACK(on_enter)    , self);
    g_signal_connect(G_OBJECT(line_edit), "focus-in-event"  , G_CALLBACK(on_focus_in) , self);
    g_signal_connect(G_OBJECT(line_edit), "focus-out-event" , G_CALLBACK(on_focus_out), self);

    self->view   = GTK_TEXT_VIEW(text_box_widget);
    self->widget = vbox       ;
    self->buffer = text_buffer;
    self->entry  = line_edit  ;

    /* Setup the tab label */
    GtkWidget *tab_label        = gtk_label_new           ( label                              );
    GtkWidget *tab_label_vbox   = gtk_box_new             ( GTK_ORIENTATION_HORIZONTAL, 0      );
    GtkWidget *tab_close_button = gtk_button_new          (                                    );
    GtkWidget *button_image     = gtk_image_new_from_stock( GTK_STOCK_CLOSE,GTK_ICON_SIZE_MENU );
    gtk_box_set_spacing (GTK_BOX(tab_label_vbox),0);

    /*TODO make it work*/
    /*   GtkRcStyle *style = gtk_rc_style_new();
         style->xthickness = 0;
         style->ythickness = 0;
         gtk_widget_modify_style(tab_close_button,style);*/
    gtk_button_set_image(GTK_BUTTON(tab_close_button),button_image);
    g_signal_connect(G_OBJECT(tab_close_button), "clicked", G_CALLBACK(on_close), self);

    /* Fill the layout ans show everything */
    gtk_box_pack_start(GTK_BOX(vbox)          , history_hbox    , TRUE , TRUE , 0);
    gtk_box_pack_start(GTK_BOX(vbox)          , hbox            , FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tab_label_vbox), tab_label       , TRUE , TRUE , 0);
    gtk_box_pack_start(GTK_BOX(tab_label_vbox), tab_close_button, FALSE, FALSE, 0);

    gtk_widget_show (tab_label       );
    gtk_widget_show (tab_close_button);
    gtk_widget_show (tab_label_vbox  );
    gtk_widget_show (vbox            );
    gtk_widget_show (scoll_area      );
    gtk_widget_show (text_box_widget );
    gtk_widget_show (history_hbox    );
    gtk_widget_show (h_left_spacer   );
    gtk_widget_show (h_right_spacer  );
    gtk_widget_show (hbox            );
    gtk_widget_show (line_edit       );
    gtk_widget_show (left_spacer     );
    gtk_widget_show (right_spacer    );

    self->index = gtk_notebook_append_page(GTK_NOTEBOOK(get_tab_box()),vbox,tab_label_vbox);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(get_tab_box()),vbox,TRUE);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(get_tab_box()),self->index);

    /* Keep track of the tab */
    if (!tabs) {
      tabs = g_hash_table_new(NULL,g_str_equal);
    }
    g_hash_table_insert(tabs,(gpointer)call_id,(gpointer)self);

    return self;
}

static message_tab *
new_text_message_common(const gchar* id, const gchar* message, const gchar *name, SFLPhoneClient *client)
{
    gtk_widget_show(get_tab_box());
    message_tab *tab = NULL;
    if (tabs)
        tab = g_hash_table_lookup(tabs, id);
    if (!tab)
        tab = create_messaging_tab_common(id, name, client);
    append_message(tab, name, message);
    return tab;
}

void
new_text_message(callable_obj_t* call, const gchar* message, SFLPhoneClient *client)
{
    gchar* label_text;
    if (g_strcmp0(call->_display_name, "") == 0)
       label_text = call->_display_name;
    else
       label_text = "Peer";
    message_tab *tab = new_text_message_common(call->_callID, message, label_text, client);
    tab->call = call;
}

void
new_text_message_conf(conference_obj_t* conf, const gchar* message,const gchar* from, SFLPhoneClient *client)
{
    disable_conference_calls(conf);
    message_tab *tab = new_text_message_common(conf->_confID, message,
                                               strlen(from) ? from : "Conference", client);
    tab->conf = conf;
}

message_tab *
create_messaging_tab(callable_obj_t* call, SFLPhoneClient *client)
{
    gchar *confID = dbus_get_conference_id(call->_callID);

    if (strlen(confID) > 0 && ((tabs && force_lookup(confID) == NULL) || !tabs)) {
        message_tab *result = create_messaging_tab_common(confID, "Conference", client);
        g_free(confID);
        return result;
    } else if (strlen(confID) > 0 && tabs) {
        message_tab *result = force_lookup(confID);
        g_free(confID);
        return result;
    } else {
        g_free(confID);
    }

    gchar* label_text;
    if (strcmp(call->_display_name,""))
       label_text = call->_display_name;
    else
       label_text = call->_peer_number ;
    message_tab* self = create_messaging_tab_common(call->_callID, label_text, client);

    self->call = call;
    self->conf = NULL;
    return self;
}

message_tab *
create_messaging_tab_conf(conference_obj_t* call, SFLPhoneClient *client)
{
    if (call->_confID && strlen(call->_confID)) {
        message_tab* self = create_messaging_tab_common(call->_confID, "Conference", client);
        self->conf = call;
        self->call = NULL;
        disable_conference_calls(call);
        return self;
    }
    return NULL;
}
