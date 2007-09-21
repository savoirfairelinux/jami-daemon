/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *                                                                              
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
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
#include <dbus.h>

GtkListStore * store;
GtkWidget *view;

GtkWidget * callButton;
GtkWidget * pickupButton;
GtkWidget * hangupButton;
GtkWidget * holdButton;
GtkWidget * transfertButton;
GtkWidget * unholdButton;

/**
 * Make a call
 */
static void 
call_button( GtkWidget *widget, gpointer   data )
{
  sflphone_new_call();
}

/**
 * Pick up
 */
static void 
pick_up( GtkWidget *widget, gpointer   data )
{
  sflphone_pick_up();
}

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
transfert( GtkWidget *widget, gpointer   data )
{
  call_t * c = (call_t*) call_list_get_by_state (CALL_STATE_CURRENT);
  if(c)
  {
    dbus_transfert(c,"124");
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
update_buttons ()
{
  gtk_widget_set_sensitive( GTK_WIDGET(callButton),       FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(pickupButton),     FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(holdButton),       FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(transfertButton),  FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(unholdButton),     FALSE);
	
	call_t * selectedCall = call_get_selected();
	if (selectedCall)
	{
    switch(selectedCall->state) 
  	{
  	  case CALL_STATE_INCOMING:
        gtk_widget_set_sensitive( GTK_WIDGET(pickupButton),     TRUE);
        gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     TRUE);
        break;
      case CALL_STATE_HOLD:
    	  gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     TRUE);
        gtk_widget_set_sensitive( GTK_WIDGET(unholdButton),     TRUE);
        gtk_widget_set_sensitive( GTK_WIDGET(callButton),       TRUE);
        break;
      case CALL_STATE_RINGING:
    	  gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     TRUE);
        break;
      case CALL_STATE_DIALING:
        gtk_widget_set_sensitive( GTK_WIDGET(pickupButton),     TRUE);
        gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     TRUE);
        gtk_widget_set_sensitive( GTK_WIDGET(callButton),       TRUE);
        break;
      case CALL_STATE_CURRENT:
        gtk_widget_show( hangupButton );
        gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     TRUE);
        gtk_widget_set_sensitive( GTK_WIDGET(holdButton),       TRUE);
        gtk_widget_set_sensitive( GTK_WIDGET(transfertButton),  TRUE);
        gtk_widget_set_sensitive( GTK_WIDGET(callButton),       TRUE);
        break;
      case CALL_STATE_BUSY:
      case CALL_STATE_FAILURE:
        gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     TRUE);
        break; 
  	  default:
  	    g_error("Should not happen!");
  	    break;
  	}
  }
  else 
  {
    gtk_widget_set_sensitive( GTK_WIDGET(callButton),       TRUE);
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
  
  update_buttons();
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
  	    g_error("Should not happen!");
  	    break;
  	}
  }
}                    

GtkWidget * 
create_call_tree (){
  GtkWidget *ret;
	GtkWidget *sw;
  GtkWidget *hbox;
	GtkWidget *image;
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
  
  /* Action button */     
  hbox = gtk_hbox_new (FALSE, 5);       
  
  callButton = gtk_button_new ();
  image = gtk_image_new_from_file( ICONS_DIR "/call.svg");
  gtk_button_set_image(GTK_BUTTON(callButton), image);
  //gtk_button_set_image_position( button, GTK_POS_TOP);
  gtk_box_pack_start (GTK_BOX (hbox), callButton, FALSE /*expand*/, FALSE /*fill*/, 0 /*padding*/);
  g_signal_connect (G_OBJECT (callButton), "clicked",
                    G_CALLBACK (call_button), NULL);

  pickupButton = gtk_button_new ();
  gtk_widget_set_state( GTK_WIDGET(pickupButton), GTK_STATE_INSENSITIVE);
  image = gtk_image_new_from_file( ICONS_DIR "/accept.svg");
  gtk_button_set_image(GTK_BUTTON(pickupButton), image);
  //gtk_button_set_image_position( button, GTK_POS_TOP);
  gtk_box_pack_start (GTK_BOX (hbox), pickupButton, FALSE /*expand*/, FALSE /*fill*/, 0 /*padding*/);
  g_signal_connect (G_OBJECT (pickupButton), "clicked",
                    G_CALLBACK (pick_up), NULL);
  
  hangupButton = gtk_button_new ();
  gtk_widget_hide( hangupButton );
  gtk_widget_set_state( GTK_WIDGET(hangupButton), GTK_STATE_INSENSITIVE);
  gtk_box_pack_start (GTK_BOX (hbox), hangupButton, FALSE /*expand*/, FALSE /*fill*/, 0 /*padding*/);
  image = gtk_image_new_from_file( ICONS_DIR "/hang_up.svg");
  gtk_button_set_image(GTK_BUTTON(hangupButton), image);
  //gtk_button_set_image_position( button, GTK_POS_TOP);
  g_signal_connect (G_OBJECT (hangupButton), "clicked",
                    G_CALLBACK (hang_up), NULL);
                      
  unholdButton = gtk_button_new ();
  image = gtk_image_new_from_file( ICONS_DIR "/unhold.svg");
  gtk_button_set_image(GTK_BUTTON(unholdButton), image);
  gtk_widget_set_state( GTK_WIDGET(unholdButton), GTK_STATE_INSENSITIVE);
  gtk_box_pack_end (GTK_BOX (hbox), unholdButton, FALSE /*expand*/, FALSE /*fill*/, 0 /*padding*/);
  g_signal_connect (G_OBJECT (unholdButton), "clicked",
                    G_CALLBACK (unhold), NULL);
             
  holdButton = gtk_button_new ();
  image = gtk_image_new_from_file( ICONS_DIR "/hold.svg");
  gtk_button_set_image(GTK_BUTTON(holdButton), image);
  gtk_widget_set_state( GTK_WIDGET(holdButton), GTK_STATE_INSENSITIVE);
  gtk_box_pack_end (GTK_BOX (hbox), holdButton, FALSE /*expand*/, FALSE /*fill*/, 0 /*padding*/);
  g_signal_connect (G_OBJECT (holdButton), "clicked",
                    G_CALLBACK (hold), NULL);

  transfertButton = gtk_button_new ();
  image = gtk_image_new_from_file( ICONS_DIR "/transfert.svg");
  gtk_button_set_image(GTK_BUTTON(transfertButton), image);
  gtk_widget_set_state( GTK_WIDGET(transfertButton), GTK_STATE_INSENSITIVE);
  gtk_box_pack_end (GTK_BOX (hbox), transfertButton, FALSE /*expand*/, FALSE /*fill*/, 0 /*padding*/);
  g_signal_connect (G_OBJECT (transfertButton), "clicked",
                    G_CALLBACK (transfert), NULL);
                    
  gtk_box_pack_start (GTK_BOX (ret), hbox, FALSE /*expand*/, FALSE /*fill*/, 0 /*padding*/);
  gtk_box_pack_start(GTK_BOX(ret), sw, TRUE, TRUE, 0);
	
	gtk_widget_show(ret); 
	
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
	update_buttons();
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
        markup = g_markup_printf_escaped("<b>%s</b>\n"
    						    "%s", 
    						    call_get_name(c), 
    						    call_get_number(c));
    		    		
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
      		default:
      		  g_error("Should not happen!");
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
  update_buttons();
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
		  g_error("Should not happen!");
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
  update_buttons();
	//return row_ref;
}
