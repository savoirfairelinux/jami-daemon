/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
#include "contacts/calltab.h"
#include "sflphone_const.h"
#include "dbus.h"
#include "actions.h"

static void
ungrab_key(guint key, GdkModifierType mask, GdkWindow *root);

static void
grab_key(guint key, GdkModifierType mask, GdkWindow *root);

// used to store accelerator config
static Accelerator* accelerators_list;

// used to store config (for dbus calls)
static GHashTable* shortcutsMap;


/*
 * XLib functions
 */

/*
 * filter used when an event is catched
 */
static GdkFilterReturn
filter_keys(const GdkXEvent *xevent, G_GNUC_UNUSED const GdkEvent *event,
            G_GNUC_UNUSED gpointer data)
{
    if (((XEvent *) xevent)->type != KeyPress)
        return GDK_FILTER_CONTINUE;

    XKeyEvent *key = (XKeyEvent *) xevent;
    GdkModifierType keystate = key->state & ~(Mod2Mask | Mod5Mask | LockMask);

    for (guint i = 0; accelerators_list[i].action; ++i) {
        if (accelerators_list[i].key == key->keycode &&
            accelerators_list[i].mask == keystate) {
            accelerators_list[i].callback(accelerators_list[i].data);
            return GDK_FILTER_REMOVE;
        }
    }

    return GDK_FILTER_CONTINUE;
}

/*
 * Callbacks
 */
static void
toggle_pick_up_hang_up_callback(SFLPhoneClient *client)
{
    callable_obj_t *call = calltab_get_selected_call(active_calltree_tab);
    conference_obj_t *conf = calltab_get_selected_conf(active_calltree_tab);

    g_debug("Shortcuts: Toggle pickup/hangup callback");

    if (call) {
        switch (call->_state) {
            case CALL_STATE_INCOMING:
            case CALL_STATE_TRANSFER:
                sflphone_pick_up(client);
                break;
            case CALL_STATE_DIALING:
            case CALL_STATE_HOLD:
            case CALL_STATE_CURRENT:
            case CALL_STATE_RINGING:
                sflphone_hang_up(client);
                break;
            default:
                break;
        }
    } else if (conf) {
        dbus_hang_up_conference(conf);
    } else {
        sflphone_pick_up(client);
    }
}

static void
pick_up_callback(gpointer data)
{
    sflphone_pick_up(data);
}

static void
hang_up_callback(gpointer data)
{
    sflphone_hang_up(data);
}

static void
toggle_hold_callback(G_GNUC_UNUSED gpointer data)
{
    callable_obj_t *call = calltab_get_selected_call(current_calls_tab);
    conference_obj_t *conf = calltab_get_selected_conf(active_calltree_tab);

    if (call) {
        switch (call->_state) {
            case CALL_STATE_CURRENT:
                sflphone_on_hold();
                break;
            case CALL_STATE_HOLD:
                sflphone_off_hold();
                break;
            default:
                break;
        }
    } else if (conf) {
        dbus_hold_conference(conf);
    } else {
        g_warning("Shortcuts: Error: No callable object selected");
    }
}

static void
popup_window_callback(SFLPhoneClient *client)
{
    gtk_widget_hide(client->win);
    gtk_widget_show(client->win);
}

static void
default_callback(G_GNUC_UNUSED gpointer data)
{
    g_warning("Shortcuts: Error: Missing shortcut callback");
}

/*
 * return callback corresponding to a specific action
 */
static void*
get_action_callback(const gchar* action)
{
    if (g_strcmp0(action, SHORTCUT_PICKUP) == 0)
        return pick_up_callback;

    if (g_strcmp0(action, SHORTCUT_HANGUP) == 0)
        return hang_up_callback;

    if (g_strcmp0(action, SHORTCUT_POPUP) == 0)
        return popup_window_callback;

    if (g_strcmp0(action, SHORTCUT_TOGGLEPICKUPHANGUP) == 0)
        return toggle_pick_up_hang_up_callback;

    if (g_strcmp0(action, SHORTCUT_TOGGLEHOLD) == 0)
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
remove_bindings()
{
    if (!accelerators_list)
        return;

    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);

    if (!screen)
        return;

    GdkWindow *root = gdk_screen_get_root_window(screen);
    gdk_window_remove_filter(root, (GdkFilterFunc) filter_keys, NULL);

    /* Unbind shortcuts */
    for (Accelerator *acl = accelerators_list; acl->action != NULL; ++acl)
        if (acl->key != 0)
            ungrab_key(acl->key, acl->mask, root);
}

/*
 * Create all bindings, using stored configuration
 */
static void
create_bindings()
{
    GdkDisplay *display = gdk_display_get_default();
    GdkScreen *screen = gdk_display_get_default_screen(display);

    if (!screen)
        return;

    GdkWindow *root = gdk_screen_get_root_window(screen);
    gdk_window_add_filter(root, (GdkFilterFunc) filter_keys, NULL);

    /* bind shortcuts */
    for (Accelerator *acl = accelerators_list; acl->action != NULL; ++acl)
        if (acl->key != 0)
            grab_key(acl->key, acl->mask, root);
}

/*
 * Initialize a specific binding
 */
static void
initialize_binding(const gchar* action, guint key, GdkModifierType mask)
{
    guint i;

    for (i = 0; accelerators_list[i].action != NULL &&
         g_strcmp0(action, accelerators_list[i].action) != 0; ++i)
        /* noop */;

    if (accelerators_list[i].action == NULL) {
        g_warning("Shortcut: Error: Cannot find corresponding action");
        return;
    }

    // update config value
    accelerators_list[i].key = key;
    accelerators_list[i].mask = mask;

    // update bindings
    create_bindings();
}

/*
 * Prepare accelerators list
 */
static void
initialize_accelerators_list(SFLPhoneClient *client)
{
    GList *shortcutsKeys = g_hash_table_get_keys(shortcutsMap);

    /* contains zero-initialized sentinel element at end of list */
    accelerators_list = g_new0(Accelerator, g_list_length(shortcutsKeys) + 1);

    guint i = 0;
    for (GList *elem = shortcutsKeys; elem; elem = elem->next) {
        gchar* action = elem->data;

        accelerators_list[i].action = g_strdup(action);
        accelerators_list[i].callback = get_action_callback(action);
        accelerators_list[i].data = client;
        accelerators_list[i].mask = 0;
        accelerators_list[i].key = 0;

        i++;
    }

    g_list_free(shortcutsKeys);
}

static void
update_shortcuts_map(const gchar* action, guint key, GdkModifierType mask)
{
    // Bindings: MASKxCODE
    gchar *buffer = g_strdup_printf("%dx%d", mask, key);
    g_hash_table_replace(shortcutsMap, g_strdup(action), buffer);
}

static void
update_bindings_data(guint accel_index, guint key, GdkModifierType mask)
{
    // we need to be sure this code is not already affected
    // to another action
    for (guint i = 0; accelerators_list[i].action != NULL; ++i) {
        if (accelerators_list[i].key == key &&
            accelerators_list[i].mask == mask &&
            accelerators_list[i].key != 0) {
            g_debug("Shortcuts: Existing mapping found %d+%d", mask, key);

            // disable old binding
            accelerators_list[i].key = 0;
            accelerators_list[i].mask = 0;

            // update config table
            update_shortcuts_map(accelerators_list[i].action, 0, 0);
        }
    }

    // store new key
    accelerators_list[accel_index].key = key;
    accelerators_list[accel_index].mask = mask;

    // update value in hashtable (used for dbus calls)
    update_shortcuts_map(accelerators_list[accel_index].action,
                         accelerators_list[accel_index].key,
                         accelerators_list[accel_index].mask);
}

/*
 * "Public" functions
 */

/*
 * Update current bindings with a new value
 */
void
shortcuts_update_bindings(guint shortcut_idx, guint key, GdkModifierType mask)
{
    // first remove all existing bindings
    remove_bindings();

    // update data
    update_bindings_data(shortcut_idx, key, mask);

    // recreate all bindings
    create_bindings();

    // update configuration
    dbus_set_shortcuts(shortcutsMap);
}

/*
 * Initialize bindings with configuration retrieved from dbus
 */
void
shortcuts_initialize_bindings(SFLPhoneClient *client)
{
    gchar* action, *maskAndKey, *token1, *token2 = NULL;
    guint mask, key = 0;

    g_debug("Shortcuts: Initialize bindings");

    // get shortcuts stored in config through dbus
    shortcutsMap = dbus_get_shortcuts();

    // initialize list of keys
    initialize_accelerators_list(client);

    // iterate through keys to initialize bindings
    GList *shortcutsKeys = g_hash_table_get_keys(shortcutsMap);

    for (GList *elem = shortcutsKeys; elem; elem = elem->next) {
        action = elem->data;
        maskAndKey = g_hash_table_lookup(shortcutsMap, action);

        token1 = strtok(maskAndKey, "x");
        token2 = strtok(NULL, "x");

        mask = 0;
        key = 0;

        // Value not setted
        if (token1 && token2) {
            g_debug("Shortcuts: token1 %s, token2 %s", token1, token2);

            mask = atoi(token1);
            key = atoi(token2);
        }

        if (key != 0)
            initialize_binding(action, key, mask);
    }
}

/*
 * Destroy bindings (called on exit)
 */
void
shortcuts_destroy_bindings()
{
    if (!accelerators_list)
        return;

    // remove bindings
    remove_bindings();

    // free pointers
    for (guint i = 0; accelerators_list[i].action != NULL; ++i)
        g_free(accelerators_list[i].action);

    free(accelerators_list);
    g_hash_table_destroy(shortcutsMap);
}

Accelerator *
shortcuts_get_list()
{
    return accelerators_list;
}

/*
 * Remove key "catcher" from GDK layer
 */
static void
ungrab_key(guint key, GdkModifierType mask, GdkWindow *root)
{
    g_debug("Shortcuts: Ungrabbing key %d+%d", mask, key);

    gdk_error_trap_push();
    Display *d = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    XID x = GDK_WINDOW_XID(root);

    XUngrabKey(d, key, mask           , x);
    XUngrabKey(d, key, mask | Mod2Mask, x);
    XUngrabKey(d, key, mask | Mod5Mask, x);
    XUngrabKey(d, key, mask | LockMask, x);
    XUngrabKey(d, key, mask | Mod2Mask | LockMask, x);
    XUngrabKey(d, key, mask | Mod2Mask | Mod5Mask, x);
    XUngrabKey(d, key, mask | Mod2Mask | LockMask, x);
    XUngrabKey(d, key, mask | Mod5Mask | LockMask, x);
    XUngrabKey(d, key, mask | Mod2Mask | Mod5Mask | LockMask, x);

    gdk_flush();

    if (gdk_error_trap_pop())
        g_warning("Shortcuts: Error: Ungrabbing key %d+%d", mask, key);
}

/*
 * Add key "catcher" to GDK layer
 */
static void
grab_key(guint key, GdkModifierType mask, GdkWindow *root)
{
    gdk_error_trap_push();

    g_debug("Shortcuts: Grabbing key %d+%d", mask, key);

    Display *d = GDK_DISPLAY_XDISPLAY(gdk_display_get_default());
    XID x = GDK_WINDOW_XID(root);

    XGrabKey(d, key, mask           , x, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(d, key, mask | Mod2Mask, x, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(d, key, mask | Mod5Mask, x, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(d, key, mask | LockMask, x, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(d, key, mask | Mod2Mask | Mod5Mask, x, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(d, key, mask | Mod2Mask | LockMask, x, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(d, key, mask | Mod5Mask | LockMask, x, True, GrabModeAsync, GrabModeAsync);
    XGrabKey(d, key, mask | Mod2Mask | Mod5Mask | LockMask, x, True, GrabModeAsync, GrabModeAsync);

    gdk_flush();

    if (gdk_error_trap_pop())
        g_warning("Shortcuts: Error: Grabbing key %d+%d", mask, key);
}
