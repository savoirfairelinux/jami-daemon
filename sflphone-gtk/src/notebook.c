/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Antoine Reversat <antoine.reversat@savoirfairelinux.com>
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
#include <stdlib.h>
#include <calltree.h>
#include <calllist.h>
#include <notebook.h>

calltab_t* calltab_init(){
	calltab_t* ret;
	ret = malloc(sizeof(calltab_t));

	ret->store = NULL;
	ret->view = NULL;
	ret->tree = NULL;
	ret->callQueue = NULL;
	ret->selectedCall = NULL;
	
	return ret;
}

void*
calltab_change(GtkNotebook* notebook,
		GtkNotebookPage* page,
		guint page_num,
		gpointer data){
	current_tab = tabs[page_num];
}

GtkWidget*
create_call_notebook(){
	GtkWidget* notebook;
	int i;

	notebook = gtk_notebook_new();

	for(i=0; i < NR_TABS; i++){
		create_call_tree(tabs[i]);
	}
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tabs[TAB_CALL]->tree, gtk_label_new("Call"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tabs[TAB_CALLED]->tree, gtk_label_new("Called"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tabs[TAB_RCVD]->tree, gtk_label_new("Received"));
	gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tabs[TAB_MISSED]->tree, gtk_label_new("Missed"));
	current_tab = tabs[TAB_CALL];
	g_signal_connect(notebook, "switch-page", G_CALLBACK(calltab_change), NULL);
	return notebook;
}
