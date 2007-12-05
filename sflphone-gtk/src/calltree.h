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
 
#ifndef __CALLTREE_H__
#define __CALLTREE_H__

#include <gtk/gtk.h>
#include <calllist.h>

/** @file calltree.h
  * @brief The GtkTreeView that list calls in the main window.
  */
GtkWidget * create_call_tree();

void toolbar_update_buttons();

void update_call_tree_add (call_t * c);
void update_call_tree (call_t * c);
void update_call_tree_remove (call_t * c);

GtkWidget * create_toolbar();

#endif 
