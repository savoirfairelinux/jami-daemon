/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Antoine Reversat <antoine.reversat@savoirfairelinux.net>
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

#include <gtk/gtk.h>
#include <calltree.h>
#include <calllist.h>
#include <notebook.h>

GtkWidget*
create_call_notebook(){
	GtkWidget* notebook;
	GtkWidget* call_tab;
	GtkWidget* called_tab;
	GtkWidget* rcvd_tab;
	GtkWidget* missed_tab;

	notebook = gtk_notebook_new();

	call_tab = create_call_tree();
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), call_tab, gtk_label_new("Call"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), called_tab, gtk_label_new("Called"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), rcvd_tab, gtk_label_new("Received"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), missed_tab, gtk_label_new("Missed"));
	return notebook;
}
