/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 8; tab-width: 8 -*- */

/* eel-gconf-extensions.h - Stuff to make GConf easier to use.

   Copyright (C) 2000, 2001 Eazel, Inc.

   The Gnome Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The Gnome Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the Gnome Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.

   Authors: Ramiro Estrugo <ramiro@eazel.com>
*/

#ifndef EEL_GCONF_EXTENSIONS_H
#define EEL_GCONF_EXTENSIONS_H

#include <glib.h>
#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#ifdef __cplusplus
BEGIN_EXTERN_C
#endif
/**
 * Gconf
 */
#define CONF_PREFIX		"/apps/sflphone-client-gnome"
#define CONF_MAIN_WINDOW_WIDTH		CONF_PREFIX "/state/window_width"
#define CONF_MAIN_WINDOW_HEIGHT		CONF_PREFIX "/state/window_height"
#define CONF_MAIN_WINDOW_POSITION_X		CONF_PREFIX "/state/window_position_x"
#define CONF_MAIN_WINDOW_POSITION_Y		CONF_PREFIX "/state/window_position_y"
#define CONF_IM_WINDOW_WIDTH		CONF_PREFIX "/state/im_width"
#define CONF_IM_WINDOW_HEIGHT		CONF_PREFIX "/state/im_height"
#define CONF_IM_WINDOW_POSITION_X		CONF_PREFIX "/state/im_position_x"
#define CONF_IM_WINDOW_POSITION_Y		CONF_PREFIX "/state/im_position_y"
/** Show/Hide the dialpad */
#define CONF_SHOW_DIALPAD			CONF_PREFIX "/state/dialpad"
#define SHOW_VOLUME_CONTROLS		CONF_PREFIX "/state/volume_controls"
#define SHOW_STATUSICON				CONF_PREFIX "/state/statusicon"
#define NOTIFY_ALL					CONF_PREFIX "/state/notify_all"
#define START_HIDDEN				CONF_PREFIX "/state/start_hidden"
#define POPUP_ON_CALL				CONF_PREFIX "/state/popup"
#define HISTORY_ENABLED				CONF_PREFIX "/state/history"
#define INSTANT_MESSAGING_ENABLED               CONF_PREFIX "/state/instant_messaging"


#define EEL_GCONF_UNDEFINED_CONNECTION 0

GConfClient *eel_gconf_client_get_global(void);

void eel_gconf_global_client_free(void);

gboolean eel_gconf_handle_error(GError **error);

void eel_gconf_set_boolean(const gchar *key, gboolean boolean_value);

gboolean eel_gconf_get_boolean(const gchar *key);

int eel_gconf_get_integer(const gchar *key);

void eel_gconf_set_integer(const gchar *key, gint value);

gfloat eel_gconf_get_float(const gchar *key);

void eel_gconf_set_float(const gchar *key, gfloat value);

gchar *eel_gconf_get_string(const gchar *key);
void eel_gconf_set_string(const gchar *key, const gchar *value);
GSList *eel_gconf_get_string_list (const gchar *key);
void eel_gconf_set_string_list(const gchar *key, const GSList *value);
gboolean eel_gconf_is_default(const gchar *key);
gboolean eel_gconf_monitor_add(const gchar *directory);
gboolean eel_gconf_monitor_remove(const gchar *directory);
void eel_gconf_suggest_sync(void);
GConfValue *eel_gconf_get_value(const gchar *key);
gboolean eel_gconf_value_is_equal(const GConfValue *a, const GConfValue *b);
void eel_gconf_set_value (const gchar *key, const GConfValue *value);
gboolean eel_gconf_key_exists(const gchar *key);
void eel_gconf_value_free(GConfValue *value);
void eel_gconf_unset(const gchar *key);

/* Functions which weren't part of the eel-gconf-extensions.h file from eel */
GSList *eel_gconf_get_integer_list(const gchar *key);
void eel_gconf_set_integer_list(const gchar *key, const GSList *slist);
void gpdf_notification_add(const gchar *key, GConfClientNotifyFunc notification_callback,
                           gpointer callback_data, GList **notifiers);
void gpdf_notification_remove(GList **notifiers);
guint eel_gconf_notification_add(const gchar *key,
                                 GConfClientNotifyFunc notification_callback,
                                 gpointer callback_data);
void eel_gconf_notification_remove(guint notification_id);

#ifdef __cplusplus
END_EXTERN_C
#endif

#endif /* EEL_GCONF_EXTENSIONS_H */
