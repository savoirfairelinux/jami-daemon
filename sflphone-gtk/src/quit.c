/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.net>
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

#include "quit.h"

#define ITERATIONS  1700

void
update_progress_bar( GtkWidget* bar )
{
  gint i;
  //gtk_grab_add( bar );

  for( i = 0 ; i < ITERATIONS ; i++)
  {
    usleep(5000);
    if( i > 700 && i < 1000)
      gtk_progress_bar_set_text( (GtkProgressBar *)bar , "Saving configuration....");
    if( i > 1000 && i < 1300 )
      gtk_progress_bar_set_text( (GtkProgressBar *)bar , "Unload DTMF key");
    if( i > 1300 && i < 1500 )
      gtk_progress_bar_set_text( (GtkProgressBar *)bar , "Unload audio driver");
    if( i > 1500 )
      gtk_progress_bar_set_text( (GtkProgressBar *)bar , "Unload audio codecs");

    gtk_progress_bar_set_pulse_step( (GtkProgressBar*) bar , 0.01 );
    gtk_progress_bar_pulse( (GtkProgressBar*)bar);
    gtk_main_iteration();
  }
  //gtk_grab_remove( bar );
}

GtkWidget*
display_progress_bar( void )
{
  GtkWidget *top;
  GtkWidget *progressBar;
  GtkWidget *vbox;

  top = gtk_window_new( GTK_WINDOW_TOPLEVEL );
  //gtk_window_set_default_size( GTK_WINDOW( top ), 260, 20);
  gtk_widget_set_size_request( top , 250, 20);
  gtk_window_set_policy (GTK_WINDOW ( top ), FALSE, FALSE, FALSE);
  gtk_window_set_position( GTK_WINDOW( top ) , GTK_WIN_POS_CENTER_ALWAYS );
  gtk_container_set_border_width(GTK_CONTAINER( top ), 0);
  gtk_window_set_deletable( GTK_WINDOW( top ) , FALSE );

  vbox = gtk_vbox_new(TRUE, 0);
  gtk_container_add(GTK_CONTAINER(top), vbox);

  progressBar = gtk_progress_bar_new();
  gtk_progress_bar_set_text( (GtkProgressBar *)progressBar , "Quiting....");
  gtk_box_pack_start( GTK_BOX( vbox ) , progressBar , TRUE , TRUE , 0);
  gtk_widget_show_all( top );
  
  update_progress_bar( progressBar );

} 

