/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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
 *
 *  Additional permission under GNU GPL version 3 section 7:
 *
 *  If you modify this program, or any covered work, by linking or
 *  combining it with the OpenSSL project's OpenSSL library (or a
 *  modified version of that library), containing parts covered by the
 *  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
 *  grants you additional permission to convey the resulting work.
 *  Corresponding Source for a non-source form of such a combination
 *  shall include the source code for the parts of OpenSSL used as well
 *  as that of the covered work.
 */
 
#ifndef __SLIDERS_H__
#define __SLIDERS_H__

#include <gtk/gtk.h>
/** @file sliders.h
  * @brief Volume sliders at the bottom of the main window.
  */

/**
 * Build the sliders widget
 * @param device  Mic or speaker
 * @return GtkWidget* The slider
 */
GtkWidget * create_slider(const gchar * device);


/** 
 * This function updates the sliders without sending the value to the server.
 * This behavior prevents an infinite loop when receiving an updated volume from
 * the server.
 * @param device The device slider to update {speaker, mic}
 * @param value The value to set [0, 1.0]
 */
void set_slider(const gchar * device, gdouble value);

#endif 
