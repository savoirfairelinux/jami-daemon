/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
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

#ifndef __TOOLBAR_H__
#define __TOOLBAR_H__

#include <call.h>
#include <gtk/gtk.h>
#include <menus.h>
#include <sflphone_const.h>

GtkWidget   * toolbar;
GtkToolItem * pickupButton;
GtkToolItem * callButton;
GtkToolItem * hangupButton;
GtkToolItem * holdButton;
GtkToolItem * transfertButton;
GtkToolItem * unholdButton;
GtkToolItem * mailboxButton;
GtkToolItem * recButton;
GtkToolItem * historyButton;
GtkToolItem * contactButton;
guint transfertButtonConnId; //The button toggled signal connection ID

/**
 * Build the toolbar
 * @return GtkWidget* The toolbar
 */
GtkWidget *
create_toolbar();

GtkWidget *
create_filter_entry();

/**
 * Update the toolbar's buttons state, according to the call state
 */
void
toolbar_update_buttons();

#endif
