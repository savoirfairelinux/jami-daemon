/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
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

#ifndef __AUDIO_CONF_H
#define __AUDIO_CONF_H

#include <actions.h>

GtkWidget* create_audio_configuration (void);
GtkWidget* create_codecs_configuration (account_t **a);

GtkWidget* api_box();
GtkWidget* alsa_box();
GtkWidget* pulse_box();
GtkWidget* codecs_box();
GtkWidget* ringtone_box();

gboolean get_api( );

#endif // __AUDIO_CONF_H
