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
GtkWidget * callButton;
GtkWidget * hangupButton;
GtkWidget * hbox;
GtkWidget * holdButton;
GtkWidget * transfertButton;


/**
 * Hold the line
 */
static void 
hold( GtkWidget *widget, gpointer   data )
{
  call_t * c = (call_t*) call_list_get_by_state (CALL_STATE_CURRENT);
  if(c)
  {
    dbus_hold (c);
  }
  
  
}

/**
 * Make a call
 */
static void 
place_call( GtkWidget *widget, gpointer   data )
{
  call_t * c = (call_t*) call_list_get_by_state (CALL_STATE_DIALING);
  if(c)
  {
    sflphone_place_call(c);
  }
}

/**
 * Hang up the line
 */
static void 
hang_up( GtkWidget *widget, gpointer   data )
{
  call_t * c = (call_t*) call_list_get_by_state (CALL_STATE_CURRENT);
  if(c)
  {
    dbus_hang_up(c);
  }
}


/**
 * Transfert the line
 */
static void 
transfert( GtkWidget *widget, gpointer   data )
{
  call_t * c = (call_t*) call_list_get_by_state (CALL_STATE_CURRENT);
  if(c)
  {
    dbus_transfert(c,"124");
  }
}

GtkWidget * 
create_screen()
{
  GtkWidget * event;
  GtkWidget * sw;
  GtkWidget *subvbox;
  GtkWidget *image;
  
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
    
  /* First row */     
  hbox = gtk_hbutton_box_new ();       
  gtk_button_box_set_spacing ( GTK_BUTTON_BOX(hbox), 10);
  gtk_button_box_set_layout ( GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_START); 

  /* Creates a new button with the label "Call". */
  callButton = gtk_button_new_with_label ("Call");
  gtk_widget_set_state( GTK_WIDGET(callButton), GTK_STATE_INSENSITIVE);
  image = gtk_image_new_from_file( PIXMAPS_DIR "/call.svg");
  gtk_button_set_image(GTK_BUTTON(callButton), image);
  //gtk_button_set_image_position( button, GTK_POS_TOP);
  gtk_box_pack_start (GTK_BOX (hbox), callButton, TRUE /*expand*/, TRUE /*fill*/, 10 /*padding*/);
  g_signal_connect (G_OBJECT (callButton), "clicked",
                    G_CALLBACK (place_call), NULL);

  /* Creates a new button with the label "Hang up". */
  hangupButton = gtk_button_new_with_label ("Hang up");
  gtk_widget_hide( hangupButton );
  gtk_widget_set_state( GTK_WIDGET(hangupButton), GTK_STATE_INSENSITIVE);
  image = gtk_image_new_from_file( PIXMAPS_DIR "/hang_up.svg");
  gtk_button_set_image(GTK_BUTTON(hangupButton), image);
  //gtk_button_set_image_position( button, GTK_POS_TOP);
  g_signal_connect (G_OBJECT (hangupButton), "clicked",
                    G_CALLBACK (hang_up), NULL);

  holdButton = gtk_button_new_with_label ("Hold");
  gtk_widget_set_state( GTK_WIDGET(holdButton), GTK_STATE_INSENSITIVE);
  image = gtk_image_new_from_file( PIXMAPS_DIR "/hold.svg");
  gtk_box_pack_end (GTK_BOX (hbox), holdButton, TRUE /*expand*/, TRUE /*fill*/, 10 /*padding*/);
  gtk_button_set_image(GTK_BUTTON(holdButton), image);
  g_signal_connect (G_OBJECT (holdButton), "clicked",
                    G_CALLBACK (hold), NULL);

  transfertButton = gtk_button_new_with_label ("Transfert");
  gtk_widget_set_state( GTK_WIDGET(transfertButton), GTK_STATE_INSENSITIVE);
  image = gtk_image_new_from_file( PIXMAPS_DIR "/transfert.svg");
  gtk_button_set_image(GTK_BUTTON(transfertButton), image);
  gtk_box_pack_end (GTK_BOX (hbox), transfertButton, TRUE /*expand*/, TRUE /*fill*/, 10 /*padding*/);
  g_signal_connect (G_OBJECT (transfertButton), "clicked",
                    G_CALLBACK (transfert), NULL);

  
  /* Pack the vbox (box) which now contains all our widgets, into the
    * main window. */
  gtk_box_pack_start (GTK_BOX (subvbox), hbox, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);

  screen_clear();
  
  return subvbox;
  
}

void 
screen_clear()
{
  gtk_label_set_markup(GTK_LABEL(label), "<big><b>Welcome to SFLPhone</b></big>\n");
  
  gtk_widget_hide( hangupButton );
  gtk_widget_show( callButton );
  gtk_widget_set_sensitive( GTK_WIDGET(callButton),       FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(holdButton),       FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(transfertButton),  FALSE);
}

void 
screen_set_call(const call_t * c)
{
  gchar * markup = g_strconcat("<big><b>", call_get_name(c), "</b></big>\n", call_get_number(c), NULL);
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);
  
  if(c->state == CALL_STATE_DIALING)
  {
    gtk_widget_hide( hangupButton );
    gtk_widget_show( callButton );
    gtk_widget_set_sensitive( GTK_WIDGET(callButton),       TRUE);
    gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     FALSE);
    gtk_widget_set_sensitive( GTK_WIDGET(holdButton),       FALSE);
    gtk_widget_set_sensitive( GTK_WIDGET(transfertButton),  FALSE);
  }
  else if (c->state == CALL_STATE_CURRENT)
  {
    gtk_widget_hide( callButton  );
    /* Hack : if hangupButton is put on the window in create_screen()
     * the hbox will request space for 4 buttons making the window larger than needed */
    gtk_box_pack_start (GTK_BOX (hbox), hangupButton, TRUE /*expand*/, TRUE /*fill*/, 10 /*padding*/);
    gtk_box_reorder_child(GTK_BOX (hbox), hangupButton, 0);
    gtk_widget_show( hangupButton );
    gtk_widget_set_sensitive( GTK_WIDGET(callButton),       FALSE);
    gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     TRUE);
    gtk_widget_set_sensitive( GTK_WIDGET(holdButton),       TRUE);
    gtk_widget_set_sensitive( GTK_WIDGET(transfertButton),  TRUE);
  }
  
}
