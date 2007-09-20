/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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
 */
 
#include <dialpad.h>
#include <screen.h>


GtkWidget * label;
GtkWidget * hbox;

GtkWidget * 
create_screen()
{
  GtkWidget * event;
  GtkWidget * sw;
  GtkWidget *subvbox;
  
  GdkColor color;
  gdk_color_parse ("white", &color);    
  
  subvbox = gtk_vbox_new ( FALSE /*homogeneous*/, 10 /*spacing*/);
  
  sw = gtk_scrolled_window_new( NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_NEVER);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_NONE);
	
	event = gtk_event_box_new ();
	gtk_widget_modify_bg (event, GTK_STATE_NORMAL, &color);
	
  label = gtk_label_new ("test");
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
  gtk_misc_set_padding(GTK_MISC(label), 5, 5);
  gtk_misc_set_alignment(GTK_MISC(label), 0,0);
  gtk_container_add (GTK_CONTAINER (event), label);
  gtk_scrolled_window_add_with_viewport (GTK_SCROLLED_WINDOW (sw), event);
  
  gtk_box_pack_start (GTK_BOX (subvbox), sw, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
    
  screen_clear();
  
  return subvbox;
}

void 
screen_clear()
{
  gtk_label_set_markup(GTK_LABEL(label), "<big><b>Welcome to SFLphone</b></big>\n");
}

void 
screen_set_call(const call_t * c)
{
  gchar * markup = g_strconcat("<big><b>", call_get_name(c), "</b></big>\n", call_get_number(c), NULL);
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);
}
