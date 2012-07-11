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

// void add_message_box(ClutterActor* stage, const char* author, const char* message)
// {
//    
// }

static GtkWidget *tab_box = NULL;
static GHashTable *tabs = NULL;

GtkWidget *get_tab_box()
{
   if (!tab_box) {
      tab_box = gtk_notebook_new();
   }
   return tab_box;
}

void new_text_message(gchar* call_id, char* message)
{
   message_tab *tab = g_hash_table_lookup(tabs,call_id);
   if (!tab)
      tab = create_messaging_tab(call_id,call_id);
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

void append_message(message_tab* self, gchar* name, gchar* message)
{
   GtkTextIter current_end,new_end;
   gtk_text_buffer_get_end_iter(self->buffer, &current_end);
   gtk_text_buffer_insert(self->buffer, &current_end, name, -1);
   gtk_text_buffer_insert(self->buffer, &current_end, ": ", -1);

   gtk_text_buffer_get_end_iter(self->buffer, &current_end);
   for (int i=0;i<strlen(name)+2;i++){
      if (!gtk_text_iter_backward_char(&current_end))
         break;
   }

   gtk_text_buffer_get_end_iter(self->buffer, &new_end);
   gtk_text_buffer_apply_tag_by_name(self->buffer, "b", &current_end, &new_end);

   gtk_text_buffer_insert(self->buffer, &new_end, message, -1);
   gtk_text_buffer_insert(self->buffer, &new_end, "\n"   , -1);
}

static gboolean on_enter(GtkEntry *entry, gpointer user_data)
{
   message_tab *tab = (message_tab*)user_data;
   append_message(tab,"Me",gtk_entry_get_text(entry));
   dbus_send_text_message(tab->call_id,gtk_entry_get_text(entry));
   gtk_entry_set_text(entry,"");
}


message_tab* create_messaging_tab(const char* call_id,const char* title)
{
    message_tab *tab = g_hash_table_lookup(tabs,call_id);
    if (tab) {

       return tab;
    }
    message_tab *self = g_new0(message_tab, 1);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkTextBuffer *text_buffer = gtk_text_buffer_new(NULL);
    gtk_text_buffer_create_tag(text_buffer, "b", "weight", PANGO_WEIGHT_BOLD,NULL);

    GtkWidget *scoll_area = gtk_scrolled_window_new(NULL,NULL);

    GtkWidget *text_box_widget = gtk_text_view_new_with_buffer(text_buffer);
    gtk_text_view_set_editable(text_box_widget,FALSE);
    gtk_text_view_set_wrap_mode(text_box_widget,GTK_WRAP_CHAR);

    gtk_container_add(scoll_area,text_box_widget);
    gtk_box_pack_start(GTK_BOX(vbox), scoll_area, TRUE, TRUE, 0);

    GtkWidget *line_edit = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(vbox), line_edit, FALSE, FALSE, 0);

    g_signal_connect(G_OBJECT(line_edit), "activate", G_CALLBACK(on_enter), self);

    self->widget = vbox;
    self->call_id = call_id;
    self->title = title;
    self->buffer = text_buffer;
    self->entry = line_edit;

    int ret = gtk_notebook_append_page(GTK_NOTEBOOK(get_tab_box()),vbox,NULL);
    gtk_widget_show (vbox);
    gtk_widget_show (scoll_area);
    gtk_widget_show (text_box_widget);
    gtk_widget_show (line_edit);

    if (!tabs) {
      tabs = g_hash_table_new(NULL,g_str_equal);
    }
    g_hash_table_insert(tabs,(gpointer)call_id,(gpointer)self);

    return self;
}
