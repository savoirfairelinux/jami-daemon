/*
 *  Copyright (C) 2012 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com>
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

/**
 * This file contains functions specific for a tab of the message pane
 */

#ifndef __MESSAGING_H__
#define __MESSAGING_H__

// #include <clutter/clutter.h>
// #include <clutter-gtk/clutter-gtk.h>
#include <gtk/gtk.h>

typedef struct {
   GtkWidget *widget;
   char *call_id;
   char *title;
   GtkTextBuffer *buffer;
   GtkWidget* entry;
} message_tab;

// void add_message_box(ClutterActor* stage, const char* author, const char* message);
message_tab* create_messaging_tab(const char* call_id,const char* title);
GtkWidget *get_tab_box();

#endif