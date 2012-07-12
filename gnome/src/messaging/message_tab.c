/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
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
#include "message_tab.h"

#include "../dbus/dbus.h"
#include <string.h>

static GtkWidget *tab_box = NULL;
static GHashTable *tabs   = NULL;

GtkWidget *get_tab_box()
{
   if (!tab_box) {
      tab_box = gtk_notebook_new();
   }
   return tab_box;
}

void append_message(message_tab* self, gchar* name, const gchar* message)
{
   GtkTextIter current_end,new_end;
   gtk_text_buffer_get_end_iter(self->buffer, &current_end);
   gtk_text_buffer_insert(self->buffer, &current_end, name, -1);
   gtk_text_buffer_insert(self->buffer, &current_end, ": ", -1);

   gtk_text_buffer_get_end_iter(self->buffer, &current_end);
   for (unsigned int i=0;i<strlen(name)+2;i++){
      if (!gtk_text_iter_backward_char(&current_end))
         break;
   }

   gtk_text_buffer_get_end_iter(self->buffer, &new_end);
   gtk_text_buffer_apply_tag_by_name(self->buffer, "b", &current_end, &new_end);

   gtk_text_buffer_insert(self->buffer, &new_end, message, -1);
   gtk_text_buffer_insert(self->buffer, &new_end, "\n"   , -1);
   gtk_text_view_scroll_to_iter(self->view,&new_end,FALSE,0,0,FALSE);
}

void new_text_message(callable_obj_t* call, const gchar* message)
{
   if (!tabs) return;
   message_tab *tab = g_hash_table_lookup(tabs,call->_callID);
   if (!tab)
      tab = create_messaging_tab(call,call->_callID);
   append_message(tab,"Peer",message);
}
void replace_markup_tag(GtkTextBuffer* text_buffer, GtkTextIter* start)
{
    GtkTextIter start_match,end_match;
    while ( gtk_text_iter_forward_search(start, "<b>", GTK_TEXT_SEARCH_TEXT_ONLY | GTK_TEXT_SEARCH_VISIBLE_ONLY, &start_match, &end_match, NULL) ) {
        gtk_text_iter_forward_search(start, "</b>", GTK_TEXT_SEARCH_TEXT_ONLY | GTK_TEXT_SEARCH_VISIBLE_ONLY, &end_match, &end_match, NULL);
        gtk_text_buffer_apply_tag_by_name(text_buffer, "b", &start_match, &end_match);
        int offset = gtk_text_iter_get_offset(&end_match);
        gtk_text_buffer_get_iter_at_offset(text_buffer, start, offset);
    }
}

static void on_enter(GtkEntry *entry, gpointer user_data)
{
   message_tab *tab = (message_tab*)user_data;
   append_message(tab,(gchar*)"Me",gtk_entry_get_text(entry));
   dbus_send_text_message(tab->call_id,gtk_entry_get_text(entry));
   gtk_entry_set_text(entry,"");
}

static void on_close(GtkWidget *button,gpointer data)
{
   message_tab *tab = (message_tab*)data;
   gtk_notebook_remove_page(GTK_NOTEBOOK(get_tab_box()),tab->index);
   g_hash_table_remove(tabs,tab->call_id);
}

//conference_obj_t
message_tab* create_messaging_tab(callable_obj_t* call,const gchar* title)
{
    message_tab *tab = g_hash_table_lookup(tabs,call->_callID);
    if (tab) {

       return tab;
    }
    message_tab *self = g_new0(message_tab, 1);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkTextBuffer *text_buffer = gtk_text_buffer_new(NULL);
    gtk_text_buffer_create_tag(text_buffer, "b", "weight", PANGO_WEIGHT_BOLD,NULL);

    GtkWidget *scoll_area = gtk_scrolled_window_new(NULL,NULL);

    GtkWidget *text_box_widget = gtk_text_view_new_with_buffer(text_buffer);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(text_box_widget),FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(text_box_widget),GTK_WRAP_CHAR);
    gtk_container_add(scoll_area,text_box_widget);

    gtk_box_pack_start(GTK_BOX(vbox), scoll_area, TRUE, TRUE, 0);

    GtkWidget *line_edit = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), line_edit, FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT(line_edit), "activate", G_CALLBACK(on_enter), self);

    self->widget = vbox;
    self->call_id = call->_callID;
    self->title = malloc(strlen(title) * sizeof(gchar*));
    strcpy(self->title,title);
    self->buffer = text_buffer;
    self->entry = line_edit;
    self->view = GTK_TEXT_VIEW(text_box_widget);
    gchar* label_text;
    if (strcmp(call->_display_name,""))
       label_text = call->_display_name;
    else
       label_text = call->_peer_number;

    GtkWidget *tab_label = gtk_label_new(label_text);
    GtkWidget *tab_label_vbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_set_spacing (GTK_BOX(tab_label_vbox),0);
    gtk_box_pack_start(GTK_BOX(tab_label_vbox), tab_label, TRUE, TRUE, 0);

    GtkWidget *tab_close_button = gtk_button_new();
    GtkWidget *button_image = gtk_image_new_from_stock(GTK_STOCK_CLOSE,GTK_ICON_SIZE_MENU);
    gtk_button_set_image(GTK_BUTTON(tab_close_button),button_image);
    gtk_box_pack_start(GTK_BOX(tab_label_vbox), tab_close_button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(tab_close_button), "clicked", G_CALLBACK(on_close), self);

    gtk_widget_show (tab_label);
    gtk_widget_show (tab_close_button);
    gtk_widget_show (tab_label_vbox);

    self->index = gtk_notebook_append_page(GTK_NOTEBOOK(get_tab_box()),vbox,tab_label_vbox);
    gtk_widget_show (vbox);
    gtk_widget_show (scoll_area);
    gtk_widget_show (text_box_widget);
    gtk_widget_show (line_edit);

    if (!tabs) {
      tabs = g_hash_table_new(NULL,g_str_equal);
    }
    g_hash_table_insert(tabs,(gpointer)call->_callID,(gpointer)self);

    return self;
}
