/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include "icon_factory.h"
#include "icons/pixmap_data.h"


static GtkIconFactory *icon_factory = NULL;

void add_icon (GtkIconFactory *factory, const gchar *stock_id, const guint8 *icon_data, GtkIconSize size)
{
	GtkIconSet *icons;
	GtkIconSource *source;
	GdkPixbuf *pixbuf;

	icons = gtk_icon_factory_lookup (factory, stock_id);
	if (!icons)
	{
		pixbuf = gdk_pixbuf_new_from_inline (-1, icon_data, FALSE, NULL);
		source = gtk_icon_source_new ();
		gtk_icon_source_set_pixbuf (source, pixbuf);
		gtk_icon_source_set_size (source, size);

		icons = gtk_icon_set_new ();
		gtk_icon_set_add_source (icons, source);

		gtk_icon_factory_add (factory, stock_id, icons);

		g_object_unref (G_OBJECT (pixbuf));
		gtk_icon_source_free (source);
		gtk_icon_set_unref (icons);
	}
	else
		DEBUG ("Icon %s already exists in factory\n", stock_id);
}

GtkIconSet* lookup_sflphone_factory (const gchar *stock_id) {

	return gtk_icon_factory_lookup (icon_factory, stock_id);
}

void register_sflphone_stock_icons (GtkIconFactory *factory)
{
	add_icon (factory, GTK_STOCK_PICKUP, gnome_stock_pickup, GTK_ICON_SIZE_SMALL_TOOLBAR);	
	add_icon (factory, GTK_STOCK_HANGUP, gnome_stock_hangup, GTK_ICON_SIZE_SMALL_TOOLBAR);	
	add_icon (factory, GTK_STOCK_DIAL, gnome_stock_dial, GTK_ICON_SIZE_SMALL_TOOLBAR);	
	add_icon (factory, GTK_STOCK_TRANSFER, gnome_stock_transfer, GTK_ICON_SIZE_SMALL_TOOLBAR);	
	add_icon (factory, GTK_STOCK_ONHOLD, gnome_stock_onhold, GTK_ICON_SIZE_SMALL_TOOLBAR);	
	add_icon (factory, GTK_STOCK_OFFHOLD, gnome_stock_offhold, GTK_ICON_SIZE_SMALL_TOOLBAR);	
	add_icon (factory, GTK_STOCK_CALL_CURRENT, gnome_stock_call_current, GTK_ICON_SIZE_SMALL_TOOLBAR);	
	add_icon (factory, GTK_STOCK_ADDRESSBOOK, gnome_stock_addressbook, GTK_ICON_SIZE_SMALL_TOOLBAR);	
	add_icon (factory, GTK_STOCK_CALLS, gnome_stock_calls, GTK_ICON_SIZE_SMALL_TOOLBAR);	
}

void init_icon_factory (void)
{
	// Init the factory
	icon_factory = gtk_icon_factory_new ();

	// Load icons
	register_sflphone_stock_icons (icon_factory);

	// Specify a default icon set
	gtk_icon_factory_add_default (icon_factory);
}


