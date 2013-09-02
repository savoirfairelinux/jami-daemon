/*
 *  Copyright (C) 2012-2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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

#include <gtk/gtk.h>
#include "callable_obj.h"
#include "conference_obj.h"
#include "sflphone_client.h"

/** An IM conversation */
typedef struct {
   GtkWidget        *widget ;
   callable_obj_t   *call   ;
   conference_obj_t *conf   ;
   gchar            *title  ;
   GtkTextBuffer    *buffer ;
   GtkTextView      *view   ;
   GtkWidget        *entry  ;
   gint             index   ;
} message_tab;

/**
 * Create a new message tab or use the existing on if the call exist
 * @param call the conversation call
 */
message_tab* create_messaging_tab(callable_obj_t* call, SFLPhoneClient *client);
message_tab* create_messaging_tab_conf(conference_obj_t* call, SFLPhoneClient *client);

/** Return the main conversation notebook */
GtkWidget *get_tab_box();

/** Add a new text message to an existng conversation or create a new one
 * @param call the call
 * @param message the new message
 */
void new_text_message(callable_obj_t *call, const gchar *message, SFLPhoneClient *client);
void new_text_message_conf(conference_obj_t *call, const gchar *message,const gchar* from, SFLPhoneClient *client);

/**
 * Display or hide messaging notebook
 */
void toogle_messaging();
void hide_messaging();
void show_messaging();
void disable_messaging_tab(const gchar *id);

/**
 * Set message tab height
 */
void set_message_tab_height(GtkPaned* _paned, int height);


#endif
