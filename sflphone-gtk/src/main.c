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
#include <calllist.h>
#include <dbus.h>
#include <mainwindow.h>

#include <gtk/gtk.h>



int
main (int argc, char *argv[])
{
  gtk_init (&argc, &argv);
  
  g_print("%s\n", PACKAGE_STRING);
  g_print("Copyright (c) 2007 Savoir-faire Linux Inc.\n");
  g_print("This is free software.  You may redistribute copies of it under the terms of\n\
the GNU General Public License <http://www.gnu.org/licenses/gpl.html>.\n\
There is NO WARRANTY, to the extent permitted by law.\n\n");
  
  if(sflphone_init())
  {
    create_main_window ();
    
    /* start the main loop */
    gtk_main ();
  }
  return 0;
}

