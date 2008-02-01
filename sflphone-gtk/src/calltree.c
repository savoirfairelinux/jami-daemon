/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc@squidy.info>
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
 */

#include <gtk/gtk.h>
#include <actions.h>
#include <calltree.h>
#include <calllist.h>
#include <menus.h>
#include <dbus.h>

GtkListStore * store;
GtkWidget *view;


GtkWidget   * toolbar;
GtkToolItem * pickupButton;
GtkToolItem * callButton;
GtkToolItem * hangupButton;
GtkToolItem * holdButton;
GtkToolItem * transfertButton;
GtkToolItem * unholdButton;
guint transfertButtonConnId; //The button toggled signal connection ID


/**
 * Show popup menu
 */
static gboolean            
popup_menu (GtkWidget *widget,
            gpointer   user_data)
{
  show_popup_menu(widget, NULL);
  return TRUE;
}            
            
static gboolean
button_pressed(GtkWidget* widget, GdkEventButton *event, gpointer user_data)
{
  if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
  {
    show_popup_menu(widget,  event);
    return TRUE;
  }
  return FALSE;
}
/**
 * Make a call
 */
	static void 
call_button( GtkWidget *widget, gpointer   data )
{
	if(call_list_get_size()>0)
		sflphone_pick_up();
	else
		sflphone_new_call();
}

/**
 * Pick up
*/
/*	static void 
pick_up( GtkWidget *widget, gpointer   data )
{
	sflphone_pick_up();
}*/

/**
 * Hang up the line
 */
	static void 
hang_up( GtkWidget *widget, gpointer   data )
{
	sflphone_hang_up();
}

/**
 * Hold the line
 */
	static void 
hold( GtkWidget *widget, gpointer   data )
{
	sflphone_on_hold();
}

/**
 * Transfert the line
 */
	static void 
transfert  (GtkToggleToolButton *toggle_tool_button,
		gpointer             user_data)
{
	gboolean up = gtk_toggle_tool_button_get_active(toggle_tool_button);
	if(up)
	{
		sflphone_set_transfert();
	}
	else
	{
		sflphone_unset_transfert();
	}
}

/**
 * Unhold call
 */
	static void 
unhold( GtkWidget *widget, gpointer   data )
{
	sflphone_off_hold();
}

	void 
toolbar_update_buttons ()
{

	gtk_widget_set_sensitive( GTK_WIDGET(callButton),       FALSE);
	gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     FALSE);
	gtk_widget_set_sensitive( GTK_WIDGET(holdButton),       FALSE);
	gtk_widget_set_sensitive( GTK_WIDGET(transfertButton),  FALSE);
	gtk_widget_set_sensitive( GTK_WIDGET(unholdButton),     FALSE);
	g_object_ref(holdButton);
	g_object_ref(unholdButton);
	gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(holdButton));
	gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(unholdButton));
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), holdButton, 3);
	g_object_ref(callButton);
	g_object_ref(pickupButton);
	gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(callButton));
        gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(pickupButton));
        gtk_toolbar_insert(GTK_TOOLBAR(toolbar), callButton, 0);
	

	gtk_signal_handler_block(GTK_OBJECT(transfertButton),transfertButtonConnId);
	gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(transfertButton), FALSE);
	gtk_signal_handler_unblock(transfertButton, transfertButtonConnId);

	call_t * selectedCall = call_get_selected();
	if (selectedCall)
	{
		switch(selectedCall->state) 
		{
			case CALL_STATE_INCOMING:
				gtk_widget_set_sensitive( GTK_WIDGET(pickupButton),     TRUE);
				gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),	TRUE);
				g_object_ref(callButton);	
				gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(callButton));
				gtk_toolbar_insert(GTK_TOOLBAR(toolbar), pickupButton, 0);
				break;
			case CALL_STATE_HOLD:
				gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     TRUE);
				gtk_widget_set_sensitive( GTK_WIDGET(unholdButton),     TRUE);
				gtk_widget_set_sensitive( GTK_WIDGET(callButton),       TRUE);
				g_object_ref(holdButton);
				gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(holdButton));
				gtk_toolbar_insert(GTK_TOOLBAR(toolbar), unholdButton, 3);
				break;
			case CALL_STATE_RINGING:
				gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     TRUE);
				gtk_widget_set_sensitive( GTK_WIDGET(callButton),     TRUE);
				break;
			case CALL_STATE_DIALING:
				gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     TRUE);
				//gtk_widget_set_sensitive( GTK_WIDGET(callButton),       TRUE);
				gtk_widget_set_sensitive( GTK_WIDGET(pickupButton),       TRUE);
				g_object_ref(callButton);
				gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(callButton));
				gtk_toolbar_insert(GTK_TOOLBAR(toolbar), pickupButton, 0);
				break;
			case CALL_STATE_CURRENT:
				gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     TRUE);
				gtk_widget_set_sensitive( GTK_WIDGET(holdButton),       TRUE);
				gtk_widget_set_sensitive( GTK_WIDGET(transfertButton),  TRUE);
				gtk_widget_set_sensitive( GTK_WIDGET(callButton),       TRUE);
				break;
			case CALL_STATE_BUSY:
			case CALL_STATE_FAILURE:
				gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     TRUE);
				break; 
			case CALL_STATE_TRANSFERT:
				gtk_signal_handler_block(GTK_OBJECT(transfertButton),transfertButtonConnId);
				gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(transfertButton), TRUE);
				gtk_signal_handler_unblock(transfertButton, transfertButtonConnId);
				gtk_widget_set_sensitive( GTK_WIDGET(callButton),       TRUE);
				gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     TRUE);
				gtk_widget_set_sensitive( GTK_WIDGET(holdButton),       TRUE);
				gtk_widget_set_sensitive( GTK_WIDGET(transfertButton),  TRUE);
				break;
			default:
				g_warning("Should not happen!");
				break;
		}
	}
	else 
	{
		if( account_list_get_size() > 0 )
		{
			gtk_widget_set_sensitive( GTK_WIDGET(callButton), TRUE);
		}
		else
		{
			gtk_widget_set_sensitive( GTK_WIDGET(callButton), FALSE);
		}
	}
}
/* Call back when the user click on a call in the list */
	static void 
selected(GtkTreeSelection *sel, GtkTreeModel *model) 
{
	GtkTreeIter  iter;
	GValue val;

	if (! gtk_tree_selection_get_selected (sel, &model, &iter))
		return;

	val.g_type = 0;
	gtk_tree_model_get_value (model, &iter, 2, &val);

	call_select((call_t*) g_value_get_pointer(&val));
	g_value_unset(&val);

	toolbar_update_buttons();
}

/* A row is activated when it is double clicked */
void  row_activated(GtkTreeView       *tree_view,
		GtkTreePath       *path,
		GtkTreeViewColumn *column,
		void * foo) 
{
	call_t * selectedCall = call_get_selected();
	if (selectedCall)
	{
		switch(selectedCall->state)  
		{
			case CALL_STATE_INCOMING:
				dbus_accept(selectedCall);
				break;
			case CALL_STATE_HOLD:
				dbus_unhold(selectedCall);
				break;
			case CALL_STATE_RINGING:
			case CALL_STATE_CURRENT:
			case CALL_STATE_BUSY:
			case CALL_STATE_FAILURE:
				break;
			case CALL_STATE_DIALING:
				sflphone_place_call (selectedCall);
				break;
			default:
				g_warning("Should not happen!");
				break;
		}
	}
}                  


GtkWidget * 
create_toolbar (){
	GtkWidget *ret;
	GtkWidget *image;

	ret = gtk_toolbar_new();
	toolbar = ret;
	
	gtk_toolbar_set_orientation(GTK_TOOLBAR(ret), GTK_ORIENTATION_HORIZONTAL);
	gtk_toolbar_set_style(GTK_TOOLBAR(ret), GTK_TOOLBAR_ICONS);

	image = gtk_image_new_from_file( ICONS_DIR "/call.svg");
	callButton = gtk_tool_button_new (image, "Place a Call");
	gtk_widget_set_tooltip_text(GTK_WIDGET(callButton), "Place a call");
	g_signal_connect (G_OBJECT (callButton), "clicked",
			G_CALLBACK (call_button), NULL);
	gtk_toolbar_insert(GTK_TOOLBAR(ret), GTK_TOOL_ITEM(callButton), -1);

	image = gtk_image_new_from_file( ICONS_DIR "/accept.svg");
	pickupButton = gtk_tool_button_new(image, "Pick up");
	gtk_widget_set_tooltip_text(GTK_WIDGET(pickupButton), "Pick up");
	gtk_widget_set_state( GTK_WIDGET(pickupButton), GTK_STATE_INSENSITIVE);
	g_signal_connect(G_OBJECT (pickupButton), "clicked", 
			G_CALLBACK (call_button), NULL);
	gtk_widget_show_all(GTK_WIDGET(pickupButton));

	image = gtk_image_new_from_file( ICONS_DIR "/hang_up.svg");
	hangupButton = gtk_tool_button_new (image, "Hang up");
	gtk_widget_set_tooltip_text(GTK_WIDGET(hangupButton), "Hang up");
	gtk_widget_set_state( GTK_WIDGET(hangupButton), GTK_STATE_INSENSITIVE);
	g_signal_connect (G_OBJECT (hangupButton), "clicked",
			G_CALLBACK (hang_up), NULL);
	gtk_toolbar_insert(GTK_TOOLBAR(ret), GTK_TOOL_ITEM(hangupButton), -1);  

	image = gtk_image_new_from_file( ICONS_DIR "/unhold.svg");
	unholdButton = gtk_tool_button_new (image, "Off Hold");
	gtk_widget_set_tooltip_text(GTK_WIDGET(unholdButton), "Off Hold");
	gtk_widget_set_state( GTK_WIDGET(unholdButton), GTK_STATE_INSENSITIVE);
	g_signal_connect (G_OBJECT (unholdButton), "clicked",
			G_CALLBACK (unhold), NULL);
	gtk_widget_show_all(GTK_WIDGET(unholdButton));

	image = gtk_image_new_from_file( ICONS_DIR "/hold.svg");
	holdButton =  gtk_tool_button_new (image, "On Hold");
	gtk_widget_set_tooltip_text(GTK_WIDGET(holdButton), "On Hold");
	gtk_widget_set_state( GTK_WIDGET(holdButton), GTK_STATE_INSENSITIVE);
	g_signal_connect (G_OBJECT (holdButton), "clicked",
			G_CALLBACK (hold), NULL);
	gtk_toolbar_insert(GTK_TOOLBAR(ret), GTK_TOOL_ITEM(holdButton), -1);

	image = gtk_image_new_from_file( ICONS_DIR "/transfert.svg");
	transfertButton = gtk_toggle_tool_button_new ();
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(transfertButton), image);
	gtk_widget_set_tooltip_text(GTK_WIDGET(transfertButton), "Transfer");
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(transfertButton), "Transfer");
	gtk_widget_set_state( GTK_WIDGET(transfertButton), GTK_STATE_INSENSITIVE);
	transfertButtonConnId = g_signal_connect (G_OBJECT (transfertButton), "toggled",
			G_CALLBACK (transfert), NULL);
	gtk_toolbar_insert(GTK_TOOLBAR(ret), GTK_TOOL_ITEM(transfertButton), -1);  

	return ret;

}  

GtkWidget * 
create_call_tree (){
	GtkWidget *ret;
	GtkWidget *sw;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;

	ret = gtk_vbox_new(FALSE, 10); 
	gtk_container_set_border_width (GTK_CONTAINER (ret), 0);

	sw = gtk_scrolled_window_new( NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);

	store = gtk_list_store_new (3, 
			GDK_TYPE_PIXBUF,// Icon 
			G_TYPE_STRING,  // Description
			G_TYPE_POINTER  // Pointer to the Object
			);

	view = gtk_tree_view_new_with_model (GTK_TREE_MODEL(store));
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(view), FALSE);
	g_signal_connect (G_OBJECT (view), "row-activated",
			G_CALLBACK (row_activated),
			NULL);

  // Connect the popup menu
	g_signal_connect (G_OBJECT (view), "popup-menu",
			G_CALLBACK (popup_menu), 
			NULL);
	g_signal_connect (G_OBJECT (view), "button-press-event",
			G_CALLBACK (button_pressed), 
			NULL);

	rend = gtk_cell_renderer_pixbuf_new();
	col = gtk_tree_view_column_new_with_attributes ("Icon",
			rend,
			"pixbuf", 0,
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(view), col);

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes ("Description",
			rend,
			"markup", 1,
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(view), col);
	g_object_unref(G_OBJECT(store));
	gtk_container_add(GTK_CONTAINER(sw), view);

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
	g_signal_connect (G_OBJECT (sel), "changed",
			G_CALLBACK (selected),
			store);

	gtk_box_pack_start(GTK_BOX(ret), sw, TRUE, TRUE, 0);

	gtk_widget_show(ret); 

	toolbar_update_buttons();

	return ret;

}

void 
update_call_tree_remove (call_t * c)
{
	GtkTreeIter iter;
	GValue val;
	call_t * iterCall;

	int nbChild = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL);
	int i;
	for( i = 0; i < nbChild; i++)
	{
		if(gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, i))
		{
			val.g_type = 0;
			gtk_tree_model_get_value (GTK_TREE_MODEL(store), &iter, 2, &val);

			iterCall = (call_t*) g_value_get_pointer(&val);
			g_value_unset(&val);

			if(iterCall == c)
			{
				gtk_list_store_remove(store, &iter);
			}
		}
	}
	call_t * selectedCall = call_get_selected();
	if(selectedCall == c)
		call_select(NULL);
	toolbar_update_buttons();
}

void 
update_call_tree (call_t * c)
{
	GdkPixbuf *pixbuf;
	GtkTreeIter iter;
	GValue val;
	call_t * iterCall;

	int nbChild = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL);
	int i;
	for( i = 0; i < nbChild; i++)
	{
		if(gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, NULL, i))
		{
			val.g_type = 0;
			gtk_tree_model_get_value (GTK_TREE_MODEL(store), &iter, 2, &val);

			iterCall = (call_t*) g_value_get_pointer(&val);
			g_value_unset(&val);

			if(iterCall == c)
			{
				// Existing call in the list
				gchar * markup;
				if(c->state == CALL_STATE_TRANSFERT)
				{
					markup = g_markup_printf_escaped("<b>%s</b>\n"
							"%s\n<i>Transfert to:</i> %s",  
							call_get_name(c), 
							call_get_number(c), 
							c->to);
				}
				else
				{
					markup = g_markup_printf_escaped("<b>%s</b>\n"
							"%s", 
							call_get_name(c), 
							call_get_number(c));
				}


				switch(c->state)
				{
					case CALL_STATE_HOLD:
						pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/hold.svg", NULL);
						break;
					case CALL_STATE_RINGING:
						pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/ring.svg", NULL);
						break;
					case CALL_STATE_CURRENT:
						pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/current.svg", NULL);
						break;
					case CALL_STATE_DIALING:
						pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/dial.svg", NULL);
						break;
					case CALL_STATE_FAILURE:
						pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/fail.svg", NULL);
						break;
					case CALL_STATE_BUSY:
						pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/busy.svg", NULL);
						break;
					case CALL_STATE_TRANSFERT:
						pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/transfert.svg", NULL);
						break;
					default:
						g_warning("Should not happen!");
				}
				//Resize it
				if(pixbuf)
				{
					if(gdk_pixbuf_get_width(pixbuf) > 32 || gdk_pixbuf_get_height(pixbuf) > 32)
					{
						pixbuf =  gdk_pixbuf_scale_simple(pixbuf, 32, 32, GDK_INTERP_BILINEAR);
					}
				}
				gtk_list_store_set(store, &iter,
						0, pixbuf, // Icon
						1, markup, // Description
						-1);

				if (pixbuf != NULL)
					g_object_unref(G_OBJECT(pixbuf));

			} 
		}

	} 
	toolbar_update_buttons();
	//return row_ref;

}

void 
update_call_tree_add (call_t * c)
{
	GdkPixbuf *pixbuf;
	GtkTreeIter iter;
	GtkTreeSelection* sel;

	// New call in the list
	gchar * markup;
	markup = g_markup_printf_escaped("<b>%s</b>\n"
			"%s", 
			call_get_name(c), 
			call_get_number(c));

	gtk_list_store_append (store, &iter);

	switch(c->state)
	{
		case CALL_STATE_INCOMING:
			pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/ring.svg", NULL);
			break;
		case CALL_STATE_DIALING:
			pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/dial.svg", NULL);
			break;
		case CALL_STATE_RINGING:
			pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/ring.svg", NULL);
			break;
		default:
			g_warning("Should not happen!");
	}

	//Resize it
	if(pixbuf)
	{
		if(gdk_pixbuf_get_width(pixbuf) > 32 || gdk_pixbuf_get_height(pixbuf) > 32)
		{
			pixbuf =  gdk_pixbuf_scale_simple(pixbuf, 32, 32, GDK_INTERP_BILINEAR);
		}
	}
	gtk_list_store_set(store, &iter,
			0, pixbuf, // Icon
			1, markup, // Description
			2, c,      // Pointer
			-1);

	if (pixbuf != NULL)
		g_object_unref(G_OBJECT(pixbuf));

	//g_free(markup);
	sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
	gtk_tree_selection_select_iter(GTK_TREE_SELECTION(sel), &iter);
	toolbar_update_buttons();
	//return row_ref;
}
