/*
 *  Copyright (C) 2008 Savoir-Faire Linux inc.
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

#include <historyfilter.h>
#include <calltree.h>

GtkWidget * filter_entry;

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
  if( SHOW_SEARCHBAR )
  {
	GValue val = {0, };
	gchar* text;
	gchar* search = (gchar*)gtk_entry_get_text(GTK_ENTRY(filter_entry));
	gtk_tree_model_get_value(GTK_TREE_MODEL(model), iter, 1, &val);
	if(G_VALUE_HOLDS_STRING(&val)){
		text = (gchar *)g_value_get_string(&val);
 	}
	if(text != NULL && g_ascii_strncasecmp(search, _("Search"), 6) != 0){
		return g_regex_match_simple(search, text, G_REGEX_CASELESS, 0);
 	}
	return TRUE;
  }
  return TRUE;
} 

void
filter_entry_changed(GtkEntry* entry, gchar* arg1, gpointer data)
{ 
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(historyButton), TRUE);
	gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(histfilter));
}

void
clear_filter_entry_if_default(GtkWidget* widget, gpointer user_data)
{
	if(g_ascii_strncasecmp(gtk_entry_get_text(GTK_ENTRY(filter_entry)), _("Search"), 6) == 0)
		gtk_entry_set_text(GTK_ENTRY(filter_entry), "");
}

GtkWidget*
create_filter_entry() 
{
	GtkWidget* image;
	GtkWidget* ret = gtk_hbox_new(FALSE, 0);
      
	filter_entry = sexy_icon_entry_new();
	//filter_entry = gtk_entry_new();
	image = gtk_image_new_from_stock( GTK_STOCK_FIND , GTK_ICON_SIZE_SMALL_TOOLBAR);
	sexy_icon_entry_set_icon( SEXY_ICON_ENTRY(filter_entry), SEXY_ICON_ENTRY_PRIMARY , GTK_IMAGE(image) ); 
	sexy_icon_entry_add_clear_button( SEXY_ICON_ENTRY(filter_entry) );
	gtk_entry_set_text(GTK_ENTRY(filter_entry), _("Search"));	
	g_signal_connect(GTK_ENTRY(filter_entry), "changed", G_CALLBACK(filter_entry_changed), NULL);
	g_signal_connect(GTK_ENTRY(filter_entry), "grab-focus", G_CALLBACK(clear_filter_entry_if_default), NULL);

	gtk_box_pack_start(GTK_BOX(ret), filter_entry, TRUE, TRUE, 0);
	return ret;
}
