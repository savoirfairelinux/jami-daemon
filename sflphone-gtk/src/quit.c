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

void
update_progress_bar( GtkWidget* bar )
{
  gdouble fraction;
  gint i;
  gint total = 2000;
  //gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR (bar) , 0.0);
  gtk_grab_add( bar );

  for( i = 0 ; i < total ; i++)
  {
    fraction = (gdouble)i / (gdouble)total ;
    usleep(5000);
    if( i > 1000 )
      gtk_progress_bar_set_text( (GtkProgressBar *)bar , "Saving configuration....");
    //gtk_progress_bar_set_fraction( GTK_PROGRESS_BAR (bar) , fraction);
    gtk_progress_bar_set_pulse_step( (GtkProgressBar*) bar , 0.01 );
    gtk_progress_bar_pulse( (GtkProgressBar*)bar);
    gtk_main_iteration();
  }
  gtk_grab_remove( bar );
}

GtkWidget*
create_progress_bar( void )
{
  g_print("Progress Bar \n");
  GtkWidget *top;
  GtkWidget *progressBar;
  GtkWidget *vbox;

  top = gtk_window_new( GTK_WINDOW_TOPLEVEL );
  gtk_window_set_default_size( GTK_WINDOW( top ), 240, 20);
  gtk_container_set_border_width(GTK_CONTAINER( top ), 0);

  vbox = gtk_vbox_new(TRUE, 0);
  gtk_container_add(GTK_CONTAINER(top), vbox);

  progressBar = gtk_progress_bar_new();
  gtk_progress_bar_set_text( (GtkProgressBar *)progressBar , "Quiting....");
  gtk_box_pack_start( GTK_BOX( vbox ) , progressBar , TRUE , TRUE , 0);
  gtk_widget_show_all( top );
  
  update_progress_bar( progressBar );

} 

void
destroy_progress_bar()
{
  //gtk_widget_destroy( progressBar );
}



