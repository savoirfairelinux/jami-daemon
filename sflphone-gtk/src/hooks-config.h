/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
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

#ifndef _HOOKS_CONFIG
#define _HOOKS_CONFIG

#include <gtk/gtk.h>
#include <glib/gtypes.h>

#include "actions.h"

G_BEGIN_DECLS

#define DEFAULT_SIP_URL_FIELD       "X-sflphone-url"
#define DEFAULT_URL_COMMAND         "x-www-browser"
#define URLHOOK_COMMAND         "URLHOOK_COMMAND"
#define URLHOOK_SIP_FIELD         "URLHOOK_SIP_FIELD"
#define URLHOOK_SIP_ENABLED         "URLHOOK_SIP_ENABLED"


typedef struct _URLHook_Config {
    gchar *sip_enabled;
    gchar *sip_field;
    gchar *command;
}URLHook_Config;

/**
 * Save the parameters through D-BUS
 */
void hooks_save_parameters (void);

void hooks_load_parameters (URLHook_Config** settings);

GtkWidget* create_hooks_settings ();

G_END_DECLS

#endif // _HOOKS_CONFIG
