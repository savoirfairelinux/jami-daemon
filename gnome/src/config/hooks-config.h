/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#ifndef _HOOKS_CONFIG
#define _HOOKS_CONFIG

#include <gtk/gtk.h>
#include <glib.h>

#include "actions.h"
#include "utils.h"

#define DEFAULT_SIP_URL_FIELD       "X-sflphone-url"
#define DEFAULT_URL_COMMAND         "xdg-open \"%s\""
#define URLHOOK_COMMAND         "URLHOOK_COMMAND"
#define URLHOOK_SIP_FIELD         "URLHOOK_SIP_FIELD"
#define URLHOOK_SIP_ENABLED         "URLHOOK_SIP_ENABLED"
#define URLHOOK_IAX2_ENABLED         "URLHOOK_IAX2_ENABLED"
#define PHONE_NUMBER_HOOK_ENABLED       "PHONE_NUMBER_HOOK_ENABLED"
#define PHONE_NUMBER_HOOK_ADD_PREFIX    "PHONE_NUMBER_HOOK_ADD_PREFIX"


typedef struct _URLHook_Config {
    gchar *sip_enabled;
    gchar *iax2_enabled;
    gchar *sip_field;
    gchar *command;
    gchar *phone_number_enabled;
    gchar *phone_number_prefix;
} URLHook_Config;

/**
 * Save the parameters through D-BUS
 */
void hooks_save_parameters(SFLPhoneClient *client);

void hooks_load_parameters (URLHook_Config** settings);

GtkWidget* create_hooks_settings(SFLPhoneClient *client);

#endif // _HOOKS_CONFIG
