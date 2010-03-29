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

#ifndef _SHORTCUTS_CONFIG
#define _SHORTCUTS_CONFIG

#include <gtk/gtk.h>
#include <glib/gtypes.h>

#include "actions.h"
#include <utils.h>

G_BEGIN_DECLS

enum
{
  ACTION = 0, MASK, VALUE, COLUMNS
};

GtkWidget*
create_shortcuts_settings ();

static void
setup_tree_view (GtkWidget *treeview);

static void
accel_edited (GtkCellRendererAccel *renderer, gchar *path, guint accel_key,
    GdkModifierType mask, guint hardware_keycode, GtkTreeView *treeview);

static void
accel_cleared (GtkCellRendererAccel *renderer, gchar *path,
    GtkTreeView *treeview);

G_END_DECLS

#endif // _SHORTCUTS_CONFIG
