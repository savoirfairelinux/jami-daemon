/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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
 
#include <config.h>
#include <actions.h>
#include <calltab.h>
#include <calllist.h> 
#include <calltree.h>
#include <configwindow.h>
#include <dialpad.h>
#include <mainwindow.h>
#include <menus.h>
#include <sliders.h>
#include <historyfilter.h>

#include <gtk/gtk.h>

/** Local variables */
GtkAccelGroup * accelGroup = NULL;
GtkWidget * window    = NULL;
GtkWidget * subvbox   = NULL;
GtkWidget * vbox   = NULL;
GtkWidget * dialpad   = NULL;
GtkWidget * speaker_control = NULL;
GtkWidget * mic_control = NULL;
GtkWidget * statusBar = NULL;
GtkWidget * filterEntry = NULL;

/**
 * Minimize the main window.
 */
static gboolean
on_delete (GtkWidget * widget, gpointer data)
{
#if GTK_CHECK_VERSION(2,10,0)
  gtk_widget_hide(GTK_WIDGET( get_main_window() ));
  set_minimized( TRUE );
#endif
  return TRUE;
}

/** Ask the user if he wants to hangup current calls */
gboolean 
main_window_ask_quit(){
  guint count = call_list_get_size(current_calls);
  GtkWidget * dialog;
  guint response;
  gchar * question;
  
  if(count == 1)
  {
    question = _("<b>There is one call in progress.</b>\nDo you still want to quit?");
  }
  else
  {
    question = _("<b>There are calls in progress.</b>\nDo you still want to quit?");
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

void
create_main_window ()
{
  GtkWidget *widget;

  window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
  gtk_container_set_border_width (GTK_CONTAINER (window), 0);
  gtk_window_set_title (GTK_WINDOW (window), PACKAGE);
  gtk_window_set_default_size (GTK_WINDOW (window), 230, 320);
  gtk_window_set_default_icon_from_file (ICONS_DIR "/sflphone.png", 
                                          NULL);
  gtk_window_set_position( GTK_WINDOW( window ) , GTK_WIN_POS_MOUSE);

  /* Connect the destroy event of the window with our on_destroy function
    * When the window is about to be destroyed we get a notificaiton and
    * stop the main GTK loop
    */
  g_signal_connect (G_OBJECT (window), "delete-event",
                    G_CALLBACK (on_delete), NULL);

  /* Create an accel group for window's shortcuts */
  accelGroup = gtk_accel_group_new ();
  gtk_window_add_accel_group(GTK_WINDOW(window), accelGroup);

  vbox = gtk_vbox_new ( FALSE /*homogeneous*/, 0 /*spacing*/);
  subvbox = gtk_vbox_new ( FALSE /*homogeneous*/, 5 /*spacing*/);
  gtk_container_set_border_width (GTK_CONTAINER(subvbox), 5);
  
  widget = create_menus();
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
  
  widget = create_toolbar();
  gtk_box_pack_start (GTK_BOX (vbox), widget, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);


  gtk_box_pack_start (GTK_BOX (vbox), current_calls->tree, TRUE /*expand*/, TRUE /*fill*/,  0 /*padding*/);
  gtk_box_pack_start (GTK_BOX (vbox), history->tree, TRUE /*expand*/, TRUE /*fill*/,  0 /*padding*/);
  
  gtk_box_pack_start (GTK_BOX (vbox), subvbox, FALSE /*expand*/, FALSE /*fill*/, 0 /*padding*/);
  
  if( SHOW_SEARCHBAR ){
    filterEntry = create_filter_entry();
    gtk_box_pack_start (GTK_BOX (subvbox), filterEntry, FALSE /*expand*/, TRUE /*fill*/,  0 /*padding*/);
    gtk_widget_show_all ( filterEntry );
  }

 if( SHOW_VOLUME ){ 
    speaker_control = create_slider("speaker");
    gtk_box_pack_end (GTK_BOX (subvbox), speaker_control, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
    gtk_widget_show_all (speaker_control);
    mic_control = create_slider("mic");
    gtk_box_pack_end (GTK_BOX (subvbox), mic_control, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
    gtk_widget_show_all (mic_control);
  }

  if( SHOW_DIALPAD ){ 
    dialpad = create_dialpad();
    gtk_box_pack_end (GTK_BOX (subvbox), dialpad, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
    gtk_widget_show_all (dialpad);
  }

  /* Status bar */
  statusBar = gtk_statusbar_new();
  gtk_box_pack_start (GTK_BOX (vbox), statusBar, FALSE /*expand*/, TRUE /*fill*/,  0 /*padding*/);
  gtk_container_add (GTK_CONTAINER (window), vbox);

  /* make sure that everything, window and label, are visible */
  gtk_widget_show_all (window);

  /* dont't show the history */
  gtk_widget_hide(history->tree);
  //gtk_widget_show(current_calls->tree);
  
  gtk_tree_view_set_model(GTK_TREE_VIEW(history->view), GTK_TREE_MODEL(histfilter));
  // Configuration wizard 
  if (account_list_get_size() == 0)
  {
#if GTK_CHECK_VERSION(2,10,0)
    build_wizard();
#else
    GtkWidget * dialog = gtk_message_dialog_new_with_markup (GTK_WINDOW(window),
                                  GTK_DIALOG_DESTROY_WITH_PARENT,
                                  GTK_MESSAGE_INFO,
                                  GTK_BUTTONS_YES_NO,
                                  "<b><big>Welcome to SFLphone!</big></b>\n\nThere are no VoIP accounts configured, would you like to edit the preferences now?");

    int response = gtk_dialog_run (GTK_DIALOG(dialog));

    gtk_widget_destroy (GTK_WIDGET(dialog));

    if (response == GTK_RESPONSE_YES)
    {
      show_config_window();
    }
#endif
  }
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
main_window_dialpad( gboolean *state ){
  if( !SHOW_DIALPAD )
  {
    dialpad = create_dialpad();
    gtk_box_pack_end (GTK_BOX (subvbox), dialpad, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
    gtk_widget_show_all (dialpad);
    *state = TRUE;
  }
  else
  {
    gtk_container_remove(GTK_CONTAINER (subvbox), dialpad);
    *state = FALSE;
  }
}

void
main_window_volume_controls( gboolean *state ){
  if( !SHOW_VOLUME )
  {
    speaker_control = create_slider("speaker");
    gtk_box_pack_end (GTK_BOX (subvbox), speaker_control, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
    gtk_widget_show_all (speaker_control);
    mic_control = create_slider("mic");
    gtk_box_pack_end (GTK_BOX (subvbox), mic_control, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
    gtk_widget_show_all (mic_control);
    *state = TRUE;
  }
  else
  {
    gtk_container_remove( GTK_CONTAINER(subvbox) , speaker_control );
    gtk_container_remove( GTK_CONTAINER(subvbox) , mic_control );
    *state = FALSE;
  }
}

void
main_window_searchbar( gboolean *state ){
  if( !SHOW_SEARCHBAR )
  {
    filterEntry = create_filter_entry();
    gtk_box_pack_start (GTK_BOX (subvbox), filterEntry, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
    gtk_widget_show_all (filterEntry);
    *state = TRUE;
  }
  else
  {
    gtk_container_remove( GTK_CONTAINER(subvbox) , filterEntry );
    *state = FALSE;
  }
}

void 
statusbar_push_message(const gchar * message, guint id)
{ 
  gtk_statusbar_push(GTK_STATUSBAR(statusBar), id, message);
}

void 
statusbar_pop_message(guint id)
{ 
  gtk_statusbar_pop(GTK_STATUSBAR(statusBar), id);
}



