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
 
#include <config.h>
#include <actions.h>
#include <calllist.h> 
#include <calltree.h>
#include <dialpad.h>
#include <menus.h>
#include <screen.h>
#include <sliders.h>

#include <gtk/gtk.h>

/** Local variables */
GtkAccelGroup * accelGroup = NULL;
GtkWidget * window   = NULL;
GtkWidget * subvbox  = NULL;
GtkWidget * dialpad  = NULL;
gboolean showDialpad = FALSE; // true if the dialpad have been showned


/**
 * Terminate the main loop.
 */
static gboolean
on_delete (GtkWidget * widget, gpointer data)
{
  /* Must return FALSE to have the window destroyed */
  return !sflphone_quit();
}

/** Ask the user if he wants to hangup current calls */
gboolean 
main_window_ask_quit(){
  guint count = call_list_get_size();
  GtkWidget * dialog;
  guint response;
  gchar * question;
  
  if(count == 1)
  {
    question = "<b>There is one call in progress.</b>\nDo you still want to quit?";
  }
  else
  {
    question = "<b>There are calls in progress.</b>\nDo you still want to quit?";
  }
  
  dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW(window) ,
                                  GTK_DIALOG_MODAL,
                                  GTK_MESSAGE_QUESTION,
                                  GTK_BUTTONS_YES_NO,
                                  question);
  
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  
  gtk_widget_destroy (dialog);
  if(response == GTK_RESPONSE_YES)
  { 
    return TRUE;
  }
  else if(response == GTK_RESPONSE_NO)
  {
    return FALSE;
  }
  return TRUE;
}

static gboolean
on_key_released (GtkWidget   *widget,
                GdkEventKey *event,
                gpointer     user_data)  
{
#ifdef DEBUG
  g_print("KEY %s, %d\n", event->string, event->keyval);
#endif 
  // If a modifier key is pressed, it's a shortcut, pass along
  if(event->state & GDK_CONTROL_MASK || 
     event->state & GDK_MOD1_MASK    ||
     event->keyval == 60             || // <
     event->keyval == 62             || // >
     event->keyval == 34             || // "
     event->keyval == 65361          || // left arrow
     event->keyval == 65363          || // right arrow
     event->keyval == 32                // space
     )
    return FALSE;
  sflphone_keypad(event->keyval, event->string);
  return TRUE;
}                

void
create_main_window ()
{
  GtkWidget *widget;
  GtkWidget *vbox;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);
  gtk_window_set_title (GTK_WINDOW (window), PACKAGE);
  gtk_window_set_default_size (GTK_WINDOW (window), 200, 300);
  gtk_window_set_default_icon_from_file (ICONS_DIR "/sflphone.png", 
                                          NULL);

  /* Connect the destroy event of the window with our on_destroy function
    * When the window is about to be destroyed we get a notificaiton and
    * stop the main GTK loop
    */
  g_signal_connect (G_OBJECT (window), "delete-event",
                    G_CALLBACK (on_delete), NULL);
  g_signal_connect (G_OBJECT (window), "key-press-event",
                    G_CALLBACK (on_key_released), NULL);

  /* Create an accel group for window's shortcuts */
  accelGroup = gtk_accel_group_new ();
  gtk_window_add_accel_group(GTK_WINDOW(window), accelGroup);

  vbox = gtk_vbox_new ( FALSE /*homogeneous*/, 0 /*spacing*/);
  subvbox = gtk_vbox_new ( FALSE /*homogeneous*/, 5 /*spacing*/);
  gtk_container_set_border_width (GTK_CONTAINER(subvbox), 5);
  
  widget = create_menus();
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
  gtk_box_pack_start (GTK_BOX (vbox), subvbox, TRUE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
 
  widget = create_screen();
  // TODO Add the screen when we are decided
  //gtk_box_pack_start (GTK_BOX (subvbox), widget, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
  
  gtk_box_pack_start (GTK_BOX (subvbox), create_call_tree(), TRUE /*expand*/, TRUE /*fill*/,  0 /*padding*/);
  
  widget = create_slider("speaker");
  gtk_box_pack_start (GTK_BOX (subvbox), widget, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
  widget = create_slider("mic");
  gtk_box_pack_start (GTK_BOX (subvbox), widget, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
  
  /* Status bar */
  widget = gtk_statusbar_new();
  gtk_box_pack_start (GTK_BOX (vbox), gtk_statusbar_new(), FALSE /*expand*/, TRUE /*fill*/,  0 /*padding*/);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  /* make sure that everything, window and label, are visible */
  gtk_widget_show_all (window);
  
  screen_clear();
}

GtkAccelGroup * 
get_accel_group()
{
  return accelGroup;
}

GtkWidget * 
get_main_window()
{
  return window;
}

void
main_window_message(GtkMessageType type, gchar * markup){
  GtkWidget * dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW(get_main_window()),
                                      GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                      type,
                                      GTK_BUTTONS_CLOSE,
                                      markup);
  gtk_dialog_run (GTK_DIALOG(dialog));
  
  gtk_widget_destroy (GTK_WIDGET(dialog));
}

void
main_window_error_message(gchar * markup){
  main_window_message(GTK_MESSAGE_ERROR, markup);
}

void
main_window_warning_message(gchar * markup){
  main_window_message(GTK_MESSAGE_WARNING, markup);
}

void
main_window_info_message(gchar * markup){
  main_window_message(GTK_MESSAGE_INFO, markup);
}

void
main_window_dialpad(gboolean show){
  if(show && !showDialpad)
  {
    dialpad = create_dialpad();
    gtk_box_pack_end (GTK_BOX (subvbox), dialpad, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
    gtk_box_reorder_child(GTK_BOX (subvbox), dialpad, 1);
    gtk_widget_show_all (dialpad);
  }
  else if (!show && showDialpad)
  {
    gtk_container_remove(GTK_CONTAINER (subvbox), dialpad);
  }
  showDialpad = show;
    
}

