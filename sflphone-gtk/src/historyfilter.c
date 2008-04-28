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
#include <historyfilter.h>

GtkTreeModel*
create_filter(GtkTreeModel* child)
{
	GtkTreeModel* ret = gtk_tree_model_filter_new(child, NULL);
	gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(ret), is_visible, NULL, NULL);
	return GTK_TREE_MODEL(ret);
}

gboolean
is_visible(GtkTreeModel* model, GtkTreeIter* iter, gpointer data)
{
	GValue val = {0, };
	gchar* text;
	gtk_tree_model_get_value(GTK_TREE_MODEL(model), iter, 1, &val);
	if(G_VALUE_HOLDS_STRING(&val)){
		text = (gchar *)g_value_get_string(&val);
	}
	if(text != NULL){
		if(g_regex_match_simple("122", text, 0, 0)){
			printf("match\n");
			return FALSE;
		}else{
			return TRUE;
		}
	}
}
