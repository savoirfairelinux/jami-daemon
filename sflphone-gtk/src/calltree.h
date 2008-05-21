/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
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
 
#ifndef __CALLTREE_H__
#define __CALLTREE_H__

#include <gtk/gtk.h>

#include <calllist.h>
#include <calltab.h>
#include <timestamp.h>


/** @file calltree.h
  * @brief The GtkTreeView that list calls in the main window.
  */

GtkToolItem * historyButton;

calltab_t* active_calltree;
/**
 * Create a new widget calltree
 * @return GtkWidget* A new widget
 */
void create_call_tree(calltab_t* tab);

/**
 * Update the toolbar's buttons state, according to the call state
 */
void toolbar_update_buttons();
static void toggle_history(GtkToggleToolButton *toggle_tool_button, gpointer user_data);

/**
 * Add a call in the calltree
 * @param c The call to add
 */
void update_call_tree_add (calltab_t* ct, call_t * c);

/*
 * Update the call tree if the call state changes
 * @param c The call to update
 */ 
void update_call_tree (calltab_t* ct, call_t * c);
/**
 * Remove a call from the call tree
 * @param c The call to remove
 */
void update_call_tree_remove (calltab_t* ct, call_t * c);

/**
 * Build the toolbar
 * @return GtkWidget* The toolbar
 */
GtkWidget * create_toolbar();

void switch_tab( void );

#endif 
