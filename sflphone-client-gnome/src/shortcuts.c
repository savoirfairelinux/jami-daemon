/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include <string.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <X11/Xlib.h>
#include <X11/XF86keysym.h>
#include <gdk/gdkx.h>
#include <dbus/dbus-glib.h>
#include <stdlib.h>
#include <stdio.h>

#include "shortcuts.h"
#include "mainwindow.h"
#include "callable_obj.h"
#include "sflphone_const.h"
#include "dbus.h"

// used to store accelerator config
static Accelerator* accelerators_list;

// used to store config (for dbus calls)
static GHashTable* shortcutsMap;

/*
 * Callbacks
 */

static void
toggle_pick_up_hang_up_callback ()
{
  callable_obj_t * selectedCall = calltab_get_selected_call (active_calltree);
  conference_obj_t * selectedConf = calltab_get_selected_conf (active_calltree);

  g_print ("toggle_pick_up_hang_up_callback\n");

  if (selectedCall)
    {
      switch (selectedCall->_state)
        {
      case CALL_STATE_INCOMING:
      case CALL_STATE_TRANSFERT:
        sflphone_pick_up ();
        break;
      case CALL_STATE_DIALING:
      case CALL_STATE_HOLD:
      case CALL_STATE_CURRENT:
      case CALL_STATE_RECORD:
      case CALL_STATE_RINGING:
        sflphone_hang_up ();
        break;
        }
    }
  else if (selectedConf)
    {
      dbus_hang_up_conference (selectedConf);
    }
  else
    sflphone_pick_up ();
}

static void
pick_up_callback ()
{
  sflphone_pick_up ();
}

static void
hang_up_callback ()
{
  sflphone_hang_up ();
}

static void
toggle_hold_callback ()
{
  callable_obj_t * selectedCall = calltab_get_selected_call (current_calls);
  conference_obj_t * selectedConf = calltab_get_selected_conf (active_calltree);

  if (selectedCall)
    {
      switch (selectedCall->_state)
        {
      case CALL_STATE_CURRENT:
      case CALL_STATE_RECORD:
        g_print ("on hold\n");
        sflphone_on_hold ();
        break;
      case CALL_STATE_HOLD:
        g_print ("off hold\n");
        sflphone_off_hold ();
        break;
        }
    }
  else if (selectedConf)
    dbus_hold_conference (selectedConf);
  else
    ERROR("Should not happen");
}

static void
popup_window_callback ()
{
  gtk_widget_hide (GTK_WIDGET(get_main_window()));
  gtk_widget_show (GTK_WIDGET(get_main_window()));
  //gtk_window_move (GTK_WINDOW (get_main_window ()),
  //    dbus_get_window_position_x (), dbus_get_window_position_y ());
}

static void
default_callback ()
{
  ERROR("Missing shortcut callback");
}

/*
 * return callback corresponding to a specific action
 */
static void*
get_action_callback (const gchar* action)
{
  if (strcmp (action, SHORTCUT_PICKUP) == 0)
    return pick_up_callback;

  if (strcmp (action, SHORTCUT_HANGUP) == 0)
    return hang_up_callback;

  if (strcmp (action, SHORTCUT_POPUP) == 0)
    return popup_window_callback;

  if (strcmp (action, SHORTCUT_TOGGLEPICKUPHANGUP) == 0)
    return toggle_pick_up_hang_up_callback;

  if (strcmp (action, SHORTCUT_TOGGLEHOLD) == 0)
    return toggle_hold_callback;

  return default_callback;
}

/*
 * Handle bindings
 */

/*
 * Remove all existing bindings
 */
static void
remove_bindings ()
{
  GdkDisplay *display = NULL;
  GdkScreen *screen = NULL;
  GdkWindow *root = NULL;
  int i, j = 0;

  display = gdk_display_get_default ();

  for (i = 0; i < gdk_display_get_n_screens (display); i++)
    {
      screen = gdk_display_get_screen (display, i);
      if (screen != NULL)
        {
          j = 0;
          root = gdk_screen_get_root_window (screen);

          // remove filter
          gdk_window_remove_filter (root, filter_keys, NULL);

          // unbind shortcuts
          while (accelerators_list[j].action != NULL)
            {
              if (accelerators_list[j].key != 0)
                {
                  ungrab_key (accelerators_list[j].key,
                      accelerators_list[j].mask, root);
                }
              j++;
            }
        }
    }
}

/*
 * Create all bindings, using stored configuration
 */
static void
create_bindings ()
{
  GdkDisplay *display;
  GdkScreen *screen;
  GdkWindow *root;
  int i, j = 0;

  display = gdk_display_get_default ();

  for (i = 0; i < gdk_display_get_n_screens (display); i++)
    {
      screen = gdk_display_get_screen (display, i);
      if (screen != NULL)
        {
          j = 0;
          root = gdk_screen_get_root_window (screen);

          // add filter
          gdk_window_add_filter (root, filter_keys, NULL);

          // bind shortcuts
          while (accelerators_list[j].action != NULL)
            {
              if (accelerators_list[j].key != 0)
                {
                  grab_key (accelerators_list[j].key,
                      accelerators_list[j].mask, root);
                }
              j++;
            }
        }
    }
}

/*
 * Initialize a specific binding
 */
static void
initialize_binding (const gchar* action, guint key, GdkModifierType mask)
{
  int i = 0;

  while (accelerators_list[i].action != NULL)
    {
      if (strcmp (action, accelerators_list[i].action) == 0)
        {
          break;
        }
      i++;
    }

  if (accelerators_list[i].action == NULL)
    {
      ERROR("Should not happen: cannot find corresponding action");
      return;
    }

  // update config value
  accelerators_list[i].key = key;
  accelerators_list[i].mask = mask;

  // update bindings
  create_bindings ();
}

/*
 * Prepare accelerators list
 */
static void
initialize_accelerators_list ()
{
  GList* shortcutsKeysElement, *shortcutsKeys = NULL;
  int i = 0;

  shortcutsKeys = g_hash_table_get_keys (shortcutsMap);

  accelerators_list = (Accelerator*) malloc (
      (g_list_length (shortcutsKeys) + 1) * sizeof(Accelerator));

  for (shortcutsKeysElement = shortcutsKeys; shortcutsKeysElement; shortcutsKeysElement
      = shortcutsKeysElement->next)
    {
      gchar* action = shortcutsKeysElement->data;

      accelerators_list[i].action = g_strdup (action);
      accelerators_list[i].callback = get_action_callback (action);
      accelerators_list[i].mask = 0;
      accelerators_list[i].key = 0;

      i++;
    }

  // last element must be null
  accelerators_list[i].action = 0;
  accelerators_list[i].callback = 0;
  accelerators_list[i].mask = 0;
  accelerators_list[i].key = 0;
}

static void
update_shortcuts_map (const gchar* action, guint key, GdkModifierType mask)
{
  gchar buffer[7];

  // Bindings: MASKxCODE
  sprintf (buffer, "%dx%d", mask, key);

  g_hash_table_replace (shortcutsMap, g_strdup (action), g_strdup (buffer));
}

static void
update_bindings_data (guint index, guint key, GdkModifierType mask)
{
  int i = 0;

  // we need to be sure this code is not already affected
  // to another action
  while (accelerators_list[i].action != NULL)
    {
      if (accelerators_list[i].key == key && accelerators_list[i].mask == mask
          && accelerators_list[i].key != 0)
        {
          DEBUG("Existing mapping found %d+%d", mask, key);

          // disable old binding
          accelerators_list[i].key = 0;
          accelerators_list[i].mask = 0;

          // update config table
          update_shortcuts_map (accelerators_list[i].action, 0, 0);
        }
      i++;
    }

  // store new key
  accelerators_list[index].key = key;
  accelerators_list[index].mask = mask;

  // update value in hashtable (used for dbus calls)
  update_shortcuts_map (accelerators_list[index].action,
      accelerators_list[index].key, accelerators_list[index].mask);
}

/*
 * "Public" functions
 */

/*
 * Update current bindings with a new value
 */
void
shortcuts_update_bindings (guint index, guint key, GdkModifierType mask)
{
  // first remove all existing bindings
  remove_bindings ();

  // update data
  update_bindings_data (index, key, mask);

  // recreate all bindings
  create_bindings ();

  // update configuration
  dbus_set_shortcuts (shortcutsMap);
}

/*
 * Initialize bindings with configuration retrieved from dbus
 */
void
shortcuts_initialize_bindings ()
{
  GList* shortcutsKeys, *shortcutsKeysElement = NULL;
  gchar* action, *maskAndKey, *token1, *token2 = NULL;
  guint mask, key = 0;

  DEBUG("Shortcuts: Initialize bindings");

  // get shortcuts stored in config through dbus
  shortcutsMap = dbus_get_shortcuts ();

  // initialize list of keys
  initialize_accelerators_list ();

  // iterate through keys to initialize bindings
  shortcutsKeys = g_hash_table_get_keys (shortcutsMap);

  for (shortcutsKeysElement = shortcutsKeys; shortcutsKeysElement; shortcutsKeysElement
      = shortcutsKeysElement->next)
    {
      action = shortcutsKeysElement->data;
      maskAndKey = g_strdup (g_hash_table_lookup (shortcutsMap, action));

      token1 = strtok (maskAndKey, "x");
      token2 = strtok (NULL, "x");

      mask = 0;
      key = 0;

      // Value not setted
      if (token1 && token2){
	DEBUG("Ahortcuts: token1 %s, token2 %s", token1, token2);
	  
	mask = atoi (token1);
	key = atoi (token2);
      }

      if (key != 0)
        initialize_binding (action, key, mask);
    }
}

/*
 * Initialize bindings with configuration retrieved from dbus
 */
void
shortcuts_destroy_bindings ()
{
  int i = 0;

  // remove bindings
  remove_bindings ();

  // free pointers
  while (accelerators_list[i].action != NULL)
    {
      g_free (accelerators_list[i].action);
      i++;
    }

  free (accelerators_list);
}

Accelerator*
shortcuts_get_list ()
{
  return accelerators_list;
}

/*
 * XLib functions
 */

/*
 * filter used when an event is catched
 */
static GdkFilterReturn
filter_keys (const GdkXEvent *xevent, const GdkEvent *event, gpointer data)
{
  XEvent *xev = NULL;
  XKeyEvent *key = NULL;
  GdkModifierType keystate = 0;
  int i = 0;

  xev = (XEvent *) xevent;
  if (xev->type != KeyPress) {
    return GDK_FILTER_CONTINUE;
  }

  key = (XKeyEvent *) xevent;
  keystate = key->state & ~(Mod2Mask | Mod5Mask | LockMask);

  // try to find corresponding action
  while (accelerators_list[i].action != NULL) {
      if (accelerators_list[i].key == key->keycode && accelerators_list[i].mask
          == keystate)
        {
          DEBUG("catched key for action: %s", accelerators_list[i].action,
              accelerators_list[i].key);

          // call associated callback function
          accelerators_list[i].callback ();

          return GDK_FILTER_REMOVE;
        }
      i++;
    }

  DEBUG("Should not be reached");

  return GDK_FILTER_CONTINUE;
}

/*
 * Remove key "catcher" from GDK layer
 */
static void
ungrab_key (guint key, GdkModifierType mask, const GdkWindow *root)
{
  DEBUG("Ungrabbing key %d+%d", mask, key);

  gdk_error_trap_push ();

  XUngrabKey (GDK_DISPLAY (), key, mask, GDK_WINDOW_XID (root));
  XUngrabKey (GDK_DISPLAY (), key, Mod2Mask | mask, GDK_WINDOW_XID (root));
  XUngrabKey (GDK_DISPLAY (), key, Mod5Mask | mask, GDK_WINDOW_XID (root));
  XUngrabKey (GDK_DISPLAY (), key, LockMask | mask, GDK_WINDOW_XID (root));
  XUngrabKey (GDK_DISPLAY (), key, Mod2Mask | Mod5Mask | mask,
      GDK_WINDOW_XID (root));
  XUngrabKey (GDK_DISPLAY (), key, Mod2Mask | LockMask | mask,
      GDK_WINDOW_XID (root));
  XUngrabKey (GDK_DISPLAY (), key, Mod5Mask | LockMask | mask,
      GDK_WINDOW_XID (root));
  XUngrabKey (GDK_DISPLAY (), key, Mod2Mask | Mod5Mask | LockMask | mask,
      GDK_WINDOW_XID (root));

  gdk_flush ();
  if (gdk_error_trap_pop ())
    {
      DEBUG ( "Error ungrabbing key %d+%d", mask, key);
    }
}

/*
 * Add key "catcher" to GDK layer
 */
static void
grab_key (guint key, GdkModifierType mask, const GdkWindow *root)
{
  gdk_error_trap_push ();

  DEBUG("Grabbing key %d+%d", mask, key);

  XGrabKey (GDK_DISPLAY (), key, mask, GDK_WINDOW_XID (root), True,
      GrabModeAsync, GrabModeAsync);
  XGrabKey (GDK_DISPLAY (), key, Mod2Mask | mask, GDK_WINDOW_XID (root), True,
      GrabModeAsync, GrabModeAsync);
  XGrabKey (GDK_DISPLAY (), key, Mod5Mask | mask, GDK_WINDOW_XID (root), True,
      GrabModeAsync, GrabModeAsync);
  XGrabKey (GDK_DISPLAY (), key, LockMask | mask, GDK_WINDOW_XID (root), True,
      GrabModeAsync, GrabModeAsync);
  XGrabKey (GDK_DISPLAY (), key, Mod2Mask | Mod5Mask | mask,
      GDK_WINDOW_XID (root), True, GrabModeAsync, GrabModeAsync);
  XGrabKey (GDK_DISPLAY (), key, Mod2Mask | LockMask | mask,
      GDK_WINDOW_XID (root), True, GrabModeAsync, GrabModeAsync);
  XGrabKey (GDK_DISPLAY (), key, Mod5Mask | LockMask | mask,
      GDK_WINDOW_XID (root), True, GrabModeAsync, GrabModeAsync);
  XGrabKey (GDK_DISPLAY (), key, Mod2Mask | Mod5Mask | LockMask | mask,
      GDK_WINDOW_XID (root), True, GrabModeAsync, GrabModeAsync);

  gdk_flush ();
  if (gdk_error_trap_pop ())
    {
      DEBUG ("Error grabbing key %d+%d", mask, key);
    }
}
