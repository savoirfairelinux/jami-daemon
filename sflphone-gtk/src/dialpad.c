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
 
#include <dialpad.h>
#include <actions.h>

/**
 * button pressed event
 */
static void
dialpad_pressed (GtkWidget * widget, gpointer data)
{
  sflphone_keypad(0, (gchar*) data);
}

GtkWidget * 
get_numpad_button (const gchar* number, gboolean twolines, const gchar * letters)
{
  GtkWidget * button;
  GtkWidget * label;
  gchar * markup;
  
  button = gtk_button_new ();
  label = gtk_label_new ( "1" );
  gtk_label_set_single_line_mode ( GTK_LABEL(label), FALSE );
  gtk_label_set_justify( GTK_LABEL(label), GTK_JUSTIFY_CENTER );
  markup = g_markup_printf_escaped("<big><b>%s</b></big>%s%s", number, (twolines == TRUE ? "\n": ""), letters);
  gtk_label_set_markup ( GTK_LABEL(label), markup);
  gtk_container_add (GTK_CONTAINER (button), label);
  g_signal_connect (G_OBJECT (button), "clicked",
                    G_CALLBACK (dialpad_pressed), (gchar*)number);
  
  return button;
}

GtkWidget * 
create_dialpad()
{
  GtkWidget * button;
  GtkWidget * table;
  
  table = gtk_table_new ( 4, 3, TRUE /* homogeneous */);
  gtk_table_set_row_spacings( GTK_TABLE(table), 5);
  gtk_table_set_col_spacings( GTK_TABLE(table), 5);
  
  button = get_numpad_button("1", TRUE, "");
  gtk_table_attach ( GTK_TABLE( table ), button, 0, 1, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  button = get_numpad_button("2", TRUE, "a b c");
  gtk_table_attach ( GTK_TABLE( table ), button, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  button = get_numpad_button("3", TRUE, "d e f");
  gtk_table_attach ( GTK_TABLE( table ), button, 2, 3, 0, 1, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  
  button = get_numpad_button("4", TRUE, "g h i");
  gtk_table_attach ( GTK_TABLE( table ), button, 0, 1, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  button = get_numpad_button("5", TRUE, "j k l");
  gtk_table_attach ( GTK_TABLE( table ), button, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  button = get_numpad_button("6", TRUE, "m n o");
  gtk_table_attach ( GTK_TABLE( table ), button, 2, 3, 1, 2, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  
  button = get_numpad_button("7", TRUE, "p q r s");
  gtk_table_attach ( GTK_TABLE( table ), button, 0, 1, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  button = get_numpad_button("8", TRUE, "t u v");
  gtk_table_attach ( GTK_TABLE( table ), button, 1, 2, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  button = get_numpad_button("9", TRUE, "w x y z");
  gtk_table_attach ( GTK_TABLE( table ), button, 2, 3, 2, 3, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  
  button = get_numpad_button("*", FALSE, "");
  gtk_table_attach ( GTK_TABLE( table ), button, 0, 1, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  button = get_numpad_button("0", FALSE, "");
  gtk_table_attach ( GTK_TABLE( table ), button, 1, 2, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  button = get_numpad_button("#", FALSE, "");
  gtk_table_attach ( GTK_TABLE( table ), button, 2, 3, 3, 4, GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  
  return table;
  
}
