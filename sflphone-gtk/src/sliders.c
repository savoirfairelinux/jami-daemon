/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc@squidy.info>
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
 */
 
#include <sliders.h>
#include <dbus.h>
#include <actions.h>
#include <string.h>

gdouble     value[2];
GtkWidget * slider[2];
GtkWidget * button[2];

// icons
GtkWidget * images[2][4];
enum device_t {
  SPEAKER = 0,
  MIKE,
  DEVICE_COUNT
} ;

enum volume_t {
  MUTED = 0,
  VOL25,
  VOL50,
  VOL75
} ;

guint toggledConnId[2]; // The button toggled signal connection ID
guint movedConnId[2];   // The slider_moved signal connection ID

void 
update_icons (int dev)
{
  float val = gtk_range_get_value(GTK_RANGE(slider[dev]));
  if(button[dev])
  {
    int icon = MUTED;
    if(val == 0)
      icon = MUTED;
    else if( val < 0.33)
      icon = VOL25;
    else if( val < 0.66)
      icon = VOL50;
    else if( val <= 1)
      icon = VOL75;
    gtk_button_set_image(GTK_BUTTON(button[dev]), GTK_WIDGET(images[dev][icon]));
  }
}

void 
slider_moved(GtkRange* range, gchar* device)
{
  gdouble value = gtk_range_get_value(range);
  g_print("Volume changed for %s: %f\n ", device, value);
  dbus_set_volume(device, value);
  if(strcmp(device, "speaker") == 0)
    update_icons(SPEAKER);
  else
    update_icons(MIKE);
}

static void 
mute_cb( GtkWidget *widget, gchar*  device )
{
  int dev;
  if(strcmp(device, "speaker") == 0)
    dev = SPEAKER;
  else
    dev = MIKE;
    
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) 
  { // Save value
    g_print("Save");
    value[dev] = gtk_range_get_value(GTK_RANGE(slider[dev]));
    dbus_set_volume(device, 0);
  }
  else 
  { //Restore value
    g_print("Restore");
    dbus_set_volume(device, value[dev]);
  }
  update_icons (dev);
}

/** This function updates the sliders without sending the value to the server.
  * This behavior prevents an infinite loop when receiving an updated volume from
  * the server.
  * @param device The device slider to update {speaker, mic}
  * @param value The value to set [0, 1.0]
  */
void 
set_slider(const gchar * device, gdouble newval)
{
  int dev;
  if(strcmp(device, "speaker") == 0)
    dev = SPEAKER;
  else
    dev = MIKE;
    
  gtk_signal_handler_block(GTK_OBJECT(slider[dev]), movedConnId[dev]);
  gtk_range_set_value(GTK_RANGE(slider[dev]), newval);
  gtk_signal_handler_unblock(slider[dev], movedConnId[dev]);
  
  gtk_signal_handler_block(GTK_OBJECT(button[dev]),toggledConnId[dev]);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button[dev]), (newval == 0 ? TRUE: FALSE));
  gtk_signal_handler_unblock(button[dev], toggledConnId[dev]);
  
  update_icons (dev);
}

/** Generates the speaker slider and mute button */
GtkWidget * 
create_slider(const gchar * device)
{ 
  // Increment the references count for the images
  // When the image is removed from a button, if the ref count = 0, then it is destroyed
  // which we don't want ;)
    
  GtkWidget * ret;
  int dev;
  
  if(strcmp(device, "speaker") == 0)
  {
    dev = SPEAKER;
    images[SPEAKER][MUTED] = gtk_image_new_from_file( ICONS_DIR "/speaker.svg");
    images[SPEAKER][VOL25] = gtk_image_new_from_file( ICONS_DIR "/speaker_25.svg");
    images[SPEAKER][VOL50] = gtk_image_new_from_file( ICONS_DIR "/speaker_50.svg");
    images[SPEAKER][VOL75] = gtk_image_new_from_file( ICONS_DIR "/speaker_75.svg");
	  g_object_ref(images[SPEAKER][MUTED]);
	  g_object_ref(images[SPEAKER][VOL25]);
	  g_object_ref(images[SPEAKER][VOL50]);
	  g_object_ref(images[SPEAKER][VOL75]);
  }
  else if (strcmp(device, "mic") == 0)
  {
    dev = MIKE;
    images[MIKE][MUTED] = gtk_image_new_from_file( ICONS_DIR "/mic.svg");
    images[MIKE][VOL25] = gtk_image_new_from_file( ICONS_DIR "/mic_25.svg");
    images[MIKE][VOL50] = gtk_image_new_from_file( ICONS_DIR "/mic_50.svg");
    images[MIKE][VOL75] = gtk_image_new_from_file( ICONS_DIR "/mic_75.svg");
	  g_object_ref(images[MIKE][MUTED]);
	  g_object_ref(images[MIKE][VOL25]);
	  g_object_ref(images[MIKE][VOL50]);
	  g_object_ref(images[MIKE][VOL75]);
  }
  
  ret = gtk_hbox_new ( FALSE /*homogeneous*/, 5 /*spacing*/);
 
  if( strcmp( device , "speaker") == 0 ) 
    gtk_widget_set_tooltip_text( GTK_WIDGET( ret ), _("Speakers volume"));
  else
    gtk_widget_set_tooltip_text( GTK_WIDGET( ret ), _("Mic volume"));
  
  button[dev] = gtk_toggle_button_new();
  gtk_box_pack_start (GTK_BOX (ret), button[dev], FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
  toggledConnId[dev] = g_signal_connect (G_OBJECT (button[dev]), "toggled",
                    G_CALLBACK (mute_cb), (gpointer)device);
  
  slider[dev] = gtk_hscale_new_with_range(0, 1, 0.05);
  gtk_scale_set_draw_value(GTK_SCALE(slider[dev]), FALSE);
  //gtk_range_set_update_policy(GTK_RANGE(slider), GTK_UPDATE_DELAYED);
  movedConnId[dev] = g_signal_connect (G_OBJECT (slider[dev]), "value_changed",
                    G_CALLBACK (slider_moved), (gpointer)device);
  gtk_box_pack_start (GTK_BOX (ret), slider[dev], TRUE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
  
  set_slider(device, dbus_get_volume(device));
  
  return ret;
}

