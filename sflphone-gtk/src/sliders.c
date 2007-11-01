/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc@squidy.info>
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
 
#include <sliders.h>
#include <dbus.h>
#include <string.h>

gdouble speakerValue;
gdouble micValue;

GtkWidget * speakerSlider;
GtkWidget * micSlider;
GtkWidget * micMuteButton;
GtkWidget * speakerMuteButton;

guint speakerToggledConnId; //The button toggled signal connection ID
guint micToggledConnId;     //The button toggled signal connection ID

guint speakerMovedConnId; //The slider_moved signal connection ID
guint micMovedConnId;     //The slider_moved signal connection ID

void slider_moved(GtkRange *range,
                    gchar * device){
  gdouble value = gtk_range_get_value(range);
  g_print("Volume changed for %s: %f\n ",device, value);
  dbus_set_volume(device, value);
}

static void 
mute_cb( GtkWidget *widget, gchar*  device )
{
  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) 
  { // Save value
    g_print("Save");
    if(strcmp(device, "speaker") == 0)
    {
      speakerValue = gtk_range_get_value(GTK_RANGE(speakerSlider));
    }
    else if (strcmp(device, "mic")== 0)
    {
      micValue = gtk_range_get_value(GTK_RANGE(micSlider));
    }
    dbus_set_volume(device, 0);
  }
  else 
  { //Restore value
    g_print("Restore");
    if(strcmp(device, "speaker") == 0)
    {
      dbus_set_volume(device, speakerValue);
    }
    else if (strcmp(device, "mic")== 0)
    {
      dbus_set_volume(device, micValue);
    }
  }
  
}

/** This function updates the sliders without sending the value to the server.
  * This behavior prevents an infinite loop when receiving an updated volume from
  * the server.
  * @param device The device slider to update {speaker, mic}
  * @param value The value to set [0, 1.0]
  */
void 
set_slider(const gchar * device, gdouble value)
{
  GtkWidget * slider;
  GtkWidget * mute;
  guint * movedConnId;
  guint * toggledConnId;
  if(strcmp(device, "speaker") == 0)
  {
    slider = speakerSlider;
    mute   = speakerMuteButton;
    movedConnId = &speakerMovedConnId;
    toggledConnId = &speakerToggledConnId;
  }
  else if (strcmp(device, "mic")== 0)
  {
    slider = micSlider;
    mute   = micMuteButton;
    movedConnId = &micMovedConnId;
    toggledConnId = &micToggledConnId;
  }
  gtk_signal_handler_block(GTK_OBJECT(slider),*movedConnId);
  gtk_range_set_value(GTK_RANGE(slider), value);
  gtk_signal_handler_unblock(slider, *movedConnId);
  
  gtk_signal_handler_block(GTK_OBJECT(mute),*toggledConnId);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(mute), (value == 0 ? TRUE: FALSE));
  gtk_signal_handler_unblock(mute, *toggledConnId);
}

/** Generates the speaker slider and mute button */
GtkWidget * 
create_slider(const gchar * device)
{ 
  GtkWidget * ret;
  GtkWidget * slider;
  GtkWidget * button;
  GtkWidget * image;
  //GtkIconTheme * icon_theme;
  //GdkPixbuf * pixbuf = NULL;
  //GError * error = NULL;
  guint * movedConnId = NULL;
  guint * toggledConnId = NULL;
  
  if(strcmp(device, "speaker") == 0)
  {
    movedConnId = &speakerMovedConnId;
    toggledConnId = &speakerToggledConnId;
    image = gtk_image_new_from_file( ICONS_DIR "/speaker.svg");
    //icon_theme = gtk_icon_theme_get_default ();
    //pixbuf = gtk_icon_theme_load_icon (icon_theme,
    //                               "audio-volume-muted", /* icon name */
    //                               16, /* size */
    //                               0,  /* flags */
    //                               &error);
    //image = gtk_image_new_from_pixbuf(pixbuf);
  }
  else if (strcmp(device, "mic") == 0)
  {
    movedConnId = &micMovedConnId;
    toggledConnId = &micToggledConnId;
    image = gtk_image_new_from_file( ICONS_DIR "/mic.svg");
  }
  
  
  ret = gtk_hbox_new ( FALSE /*homogeneous*/, 5 /*spacing*/);
  
  button = gtk_toggle_button_new();
  gtk_box_pack_start (GTK_BOX (ret), button, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
  *toggledConnId = g_signal_connect (G_OBJECT (button), "toggled",
                    G_CALLBACK (mute_cb), (gpointer)device);
  
  if (image)
  {
    gtk_button_set_image(GTK_BUTTON(button), image);
  }
  else
  {
    g_warning ("Couldn't load icon");
  }
  
  slider = gtk_hscale_new_with_range(0, 1, 0.05);
  gtk_scale_set_draw_value(GTK_SCALE(slider), FALSE);
  //gtk_range_set_update_policy(GTK_RANGE(slider), GTK_UPDATE_DELAYED);
  *movedConnId = g_signal_connect (G_OBJECT (slider), "value_changed",
                    G_CALLBACK (slider_moved), (gpointer)device);
  gtk_box_pack_start (GTK_BOX (ret), slider, TRUE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
  
  if(strcmp(device, "speaker") == 0)
  {
    speakerSlider = slider;
    speakerMuteButton = button;
  }
  else if (strcmp(device, "mic") == 0)
  {
    micSlider = slider;
    micMuteButton = button;
  }
  
  set_slider(device, dbus_get_volume(device));
  
  return ret;
}

