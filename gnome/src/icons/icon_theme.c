/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Patrick Keroulas  <patrick.keroulas@savoirfairelinux.com>
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

#include "icon_theme.h"
#include <gtk/gtk.h>
#include "icons/pixmap_data.h"

static GtkIconTheme *icon_theme = NULL;

void add_icon(GtkIconTheme *theme, const gchar *icon_name, const guint8 *icon_data, GtkIconSize size)
{
    if (gtk_icon_theme_has_icon (theme, icon_name) == FALSE) // no matter the size is
    {
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_inline(-1, icon_data, FALSE, NULL);
        gtk_icon_theme_add_builtin_icon(icon_name, size, pixbuf); // theme is no arg (?)
        g_object_unref(G_OBJECT(pixbuf));
    }
    else
        g_debug("Icon %s already exists in theme\n", icon_name);
}

void register_sflphone_stock_icons(GtkIconTheme *theme)
{
    add_icon(theme, GTK_STOCK_PICKUP, gnome_stock_pickup, 0);
    add_icon(theme, GTK_STOCK_HANGUP, gnome_stock_hangup, 0);
    add_icon(theme, GTK_STOCK_DIAL, gnome_stock_dial, 0);
    add_icon(theme, GTK_STOCK_TRANSFER, gnome_stock_transfer, 0);
    add_icon(theme, GTK_STOCK_ONHOLD, gnome_stock_onhold, 0);
    add_icon(theme, GTK_STOCK_OFFHOLD, gnome_stock_offhold, 0);
    add_icon(theme, GTK_STOCK_IM, gnome_stock_im, 0);
    add_icon(theme, GTK_STOCK_CALL_CURRENT, gnome_stock_call_current, 0);
    add_icon(theme, GTK_STOCK_ADDRESSBOOK, gnome_stock_addressbook, 0);
    add_icon(theme, GTK_STOCK_CALLS, gnome_stock_calls, 0);
    add_icon(theme, GTK_STOCK_SFLPHONE, gnome_stock_sflphone, 0);
    add_icon(theme, GTK_STOCK_FAIL, gnome_stock_fail, 0);
    add_icon(theme, GTK_STOCK_USER, gnome_stock_user, 0);
    add_icon(theme, GTK_STOCK_SCREENSHARING, gnome_stock_screensharing, 0);
    add_icon(theme, GTK_STOCK_RECORD, gnome_stock_record, 0);
    add_icon(theme, GTK_STOCK_HISTORY, gnome_stock_history, 0);
    add_icon(theme, GTK_STOCK_VOICEMAIL, gnome_stock_voicemail, 0);
    add_icon(theme, GTK_STOCK_NEWVOICEMAIL, gnome_stock_newvoicemail, 0);
    add_icon(theme, GTK_STOCK_PREFS, gnome_stock_prefs, 0);
    add_icon(theme, GTK_STOCK_PREFS_AUDIO, gnome_stock_prefs_audio, 0);
    add_icon(theme, GTK_STOCK_PREFS_VIDEO, gnome_stock_prefs_video, 0);
    add_icon(theme, GTK_STOCK_PREFS_SHORTCUT, gnome_stock_prefs_shortcut, 0);
    add_icon(theme, GTK_STOCK_PREFS_HOOK, gnome_stock_prefs_hook, 0);
    add_icon(theme, GTK_STOCK_PREFS_ADDRESSBOOK, gnome_stock_prefs_addressbook, 0);
}

void init_icon_theme(void)
{
    icon_theme = gtk_icon_theme_new();
    register_sflphone_stock_icons(icon_theme);
}
