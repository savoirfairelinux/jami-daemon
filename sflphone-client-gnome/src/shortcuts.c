/*
 *  Copyright (C) 2010 Savoir-Faire Linux inc.
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
  gtk_window_move (GTK_WINDOW (get_main_window ()),
      dbus_get_window_position_x (), dbus_get_window_position_y ());
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
  if (strcmp (action, "pick_up") == 0)
    return pick_up_callback;

  if (strcmp (action, "hang_up") == 0)
    return hang_up_callback;

  if (strcmp (action, "popup_window") == 0)
    return popup_window_callback;

  if (strcmp (action, "toggle_pick_up_hang_up") == 0)
    return toggle_pick_up_hang_up_callback;

  if (strcmp (action, "toggle_hold") == 0)
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
  GdkDisplay *display;
  GdkScreen *screen;
  GdkWindow *root;

  display = gdk_display_get_default ();

  int i = 0;
  int j = 0;
  while (accelerators_list[i].action != NULL)
    {
      if (accelerators_list[i].value != 0)
        {
          for (j = 0; j < gdk_display_get_n_screens (display); j++)
            {
              screen = gdk_display_get_screen (display, j);

              if (screen != NULL)
                {
                  root = gdk_screen_get_root_window (screen);
                  ungrab_key (accelerators_list[i].value, accelerators_list[i].mask, root);
                  gdk_window_remove_filter (root, filter_keys, NULL);
                }
            }
        }

      i++;
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

  display = gdk_display_get_default ();

  int i = 0;
  int j = 0;
  while (accelerators_list[i].action != NULL)
    {
      if (accelerators_list[i].value != 0)
        {
          // updated GDK bindings
          for (j = 0; j < gdk_display_get_n_screens (display); j++)
            {
              screen = gdk_display_get_screen (display, j);

              if (screen != NULL)
                {
                  root = gdk_screen_get_root_window (screen);
                  grab_key (accelerators_list[i].value,
                      accelerators_list[i].mask, root);
                  gdk_window_add_filter (root, filter_keys, NULL);
                }
            }
        }

      i++;
    }
}

/*
 * Initialize a specific binding
 */
static void
initialize_binding (const gchar* action, const guint code,
    const GdkModifierType mask)
{
  //initialize_shortcuts_keys();
  int index = 0;
  while (accelerators_list[index].action != NULL)
    {
      if (strcmp (action, accelerators_list[index].action) == 0)
        {
          break;
        }
      index++;
    }

  if (accelerators_list[index].action == NULL)
    {
      ERROR("Should not happen: cannot find corresponding action");
      return;
    }

  // update config value
  accelerators_list[index].value = code;
  accelerators_list[index].mask = mask;

  // update bindings
  create_bindings ();
}

/*
 * Prepare accelerators list
 */
static void
initialize_accelerators_list ()
{
  GList* shortcutsKeys = g_hash_table_get_keys (shortcutsMap);

  accelerators_list = (Accelerator*) malloc (
      (g_list_length (shortcutsKeys) + 1) * sizeof(Accelerator));

  GList* shortcutsKeysElement;
  int index = 0;
  for (shortcutsKeysElement = shortcutsKeys; shortcutsKeysElement; shortcutsKeysElement
      = shortcutsKeysElement->next)
    {
      gchar* action = shortcutsKeysElement->data;

      accelerators_list[index].action = g_strdup (action);
      accelerators_list[index].callback = get_action_callback (action);
      accelerators_list[index].mask = 0;
      accelerators_list[index].value = 0;

      index++;
    }

  // last element must be null
  accelerators_list[index].action = 0;
  accelerators_list[index].callback = 0;
  accelerators_list[index].mask = 0;
  accelerators_list[index].value = 0;
}

static void
update_shortcuts_map (const gchar* action, guint value, GdkModifierType mask)
{
  gchar buffer[7];

  // Bindings: MASKxCODE
  sprintf (buffer, "%dx%d", mask, value);

  g_hash_table_replace (shortcutsMap, g_strdup (action), g_strdup (buffer));
}

static void
update_bindings_data (const guint index, const guint code, GdkModifierType mask)
{
  // we need to be sure this code is not already affected
  // to another action
  int i = 0;
  while (accelerators_list[i].action != NULL)
    {
      if (accelerators_list[i].value == code && accelerators_list[i].mask
          == mask && accelerators_list[i].value != 0)
        {
          DEBUG("Existing mapping found - code: %d - mask: %d", code, mask);

          // disable old binding
          accelerators_list[i].value = 0;
          accelerators_list[i].mask = 0;

          // update config table
          update_shortcuts_map (accelerators_list[i].action, 0, 0);
        }
      i++;
    }

  // store new value
  accelerators_list[index].value = code;
  accelerators_list[index].mask = mask;

  // update value in hashtable (used for dbus calls)
  update_shortcuts_map (accelerators_list[index].action,
      accelerators_list[index].value, accelerators_list[index].mask);
}

/*
 * "Public" functions
 */

/*
 * Update current bindings with a new value
 */
void
shortcuts_update_bindings (const guint index, const guint code,
    GdkModifierType mask)
{
  // first remove all existing bindings
  remove_bindings ();

  // update data
  update_bindings_data (index, code, mask);

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
  // get shortcuts stored in config through dbus
  shortcutsMap = dbus_get_shortcuts ();

  // initialize list of keys
  initialize_accelerators_list ();

  // iterate through keys to initialize bindings
  GList* shortcutsKeys = g_hash_table_get_keys (shortcutsMap);
  GList* shortcutsKeysElement;

  for (shortcutsKeysElement = shortcutsKeys; shortcutsKeysElement; shortcutsKeysElement
      = shortcutsKeysElement->next)
    {
      gchar* key = shortcutsKeysElement->data;
      gchar* shortcut = g_strdup (g_hash_table_lookup (shortcutsMap, key));

      gchar* token1 = strtok (shortcut, "x");
      gchar* token2 = strtok (NULL, "x");

      guint mask = 0;
      guint value = 0;

      // Value not setted
      if (token1 == NULL)
        {
          // Nothing to do
        }
      // Backward compatibility (no mask defined)
      if (token1 != NULL && token2 == NULL)
        {
          value = atoi (token1);
          mask = 0;
        }
      // Regular case
      else
        {
          mask = atoi (token1);
          value = atoi (token2);
        }

      if (value != 0)
        initialize_binding (key, value, mask);
    }
}

/*
 * Initialize bindings with configuration retrieved from dbus
 */
void
shortcuts_destroy_bindings ()
{
  // remove bindings
  remove_bindings ();

  // free pointers
  int index = 0;
  while (accelerators_list[index].action != NULL)
    {
      g_free (accelerators_list[index].action);
      index++;
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
filter_keys (GdkXEvent *xevent, GdkEvent *event, gpointer data)
{
  XEvent *xev;
  XKeyEvent *key;

  xev = (XEvent *) xevent;
  if (xev->type != KeyPress)
    {
      return GDK_FILTER_CONTINUE;
    }

  key = (XKeyEvent *) xevent;
  GdkModifierType keystate = key->state & ~(Mod2Mask | Mod5Mask | LockMask);

  // try to find corresponding action
  int i = 0;
  while (accelerators_list[i].action != NULL)
    {
      if (accelerators_list[i].value == key->keycode
          && accelerators_list[i].mask == keystate)
        {
          DEBUG("catched key for action: %s (%d)", accelerators_list[i].action,
              accelerators_list[i].value);

          // call associated callback function
          accelerators_list[i].callback ();

          return GDK_FILTER_REMOVE;
        }
      i++;
    }

  DEBUG("Should not be reached :(\n");
  return GDK_FILTER_CONTINUE;
}

/*
 * Remove key "catcher" from GDK layer
 */
static void
ungrab_key (int key_code, GdkModifierType mask, GdkWindow *root)
{
  DEBUG("Ungrabbing key %d+%d", mask, key_code);

  gdk_error_trap_push ();

  XUngrabKey (GDK_DISPLAY (), key_code, mask, GDK_WINDOW_XID (root));
  XUngrabKey (GDK_DISPLAY (), key_code, Mod2Mask | mask, GDK_WINDOW_XID (root));
  XUngrabKey (GDK_DISPLAY (), key_code, Mod5Mask | mask, GDK_WINDOW_XID (root));
  XUngrabKey (GDK_DISPLAY (), key_code, LockMask | mask, GDK_WINDOW_XID (root));
  XUngrabKey (GDK_DISPLAY (), key_code, Mod2Mask | Mod5Mask | mask,
      GDK_WINDOW_XID (root));
  XUngrabKey (GDK_DISPLAY (), key_code, Mod2Mask | LockMask | mask,
      GDK_WINDOW_XID (root));
  XUngrabKey (GDK_DISPLAY (), key_code, Mod5Mask | LockMask | mask,
      GDK_WINDOW_XID (root));
  XUngrabKey (GDK_DISPLAY (), key_code, Mod2Mask | Mod5Mask | LockMask | mask,
      GDK_WINDOW_XID (root));

  gdk_flush ();
  if (gdk_error_trap_pop ())
    {
      DEBUG ( "Error ungrabbing key %d+%d", mask, key_code);
    }
}

/*
 * Add key "catcher" to GDK layer
 */
static void
grab_key (int key_code, GdkModifierType mask, GdkWindow *root)
{
  gdk_error_trap_push ();

  DEBUG("Grabbing key %d+%d", mask, key_code);

  XGrabKey (GDK_DISPLAY (), key_code, mask, GDK_WINDOW_XID (root), True,
      GrabModeAsync, GrabModeAsync);
  XGrabKey (GDK_DISPLAY (), key_code, Mod2Mask | mask, GDK_WINDOW_XID (root),
      True, GrabModeAsync, GrabModeAsync);
  XGrabKey (GDK_DISPLAY (), key_code, Mod5Mask | mask, GDK_WINDOW_XID (root),
      True, GrabModeAsync, GrabModeAsync);
  XGrabKey (GDK_DISPLAY (), key_code, LockMask | mask, GDK_WINDOW_XID (root),
      True, GrabModeAsync, GrabModeAsync);
  XGrabKey (GDK_DISPLAY (), key_code, Mod2Mask | Mod5Mask | mask,
      GDK_WINDOW_XID (root), True, GrabModeAsync, GrabModeAsync);
  XGrabKey (GDK_DISPLAY (), key_code, Mod2Mask | LockMask | mask,
      GDK_WINDOW_XID (root), True, GrabModeAsync, GrabModeAsync);
  XGrabKey (GDK_DISPLAY (), key_code, Mod5Mask | LockMask | mask,
      GDK_WINDOW_XID (root), True, GrabModeAsync, GrabModeAsync);
  XGrabKey (GDK_DISPLAY (), key_code, Mod2Mask | Mod5Mask | LockMask | mask,
      GDK_WINDOW_XID (root), True, GrabModeAsync, GrabModeAsync);

  gdk_flush ();
  if (gdk_error_trap_pop ())
    {
      DEBUG ("Error grabbing key %d+%d", mask, key_code);
    }
}
