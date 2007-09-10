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
#include <calltree.h>
#include <calllist.h>

GtkListStore * store;
GtkWidget * acceptButton;
GtkWidget * refuseButton;
GtkWidget * unholdButton;
GtkWidget * holdButton;
GtkWidget * callButton;
GtkWidget * hangupButton;
GtkWidget * transfertButton;

call_t * selectedCall;


/**
 * Hold the line
 */
static void 
hold( GtkWidget *widget, gpointer   data )
{
  call_t * c = (call_t*) call_list_get_by_state (CALL_STATE_CURRENT);
  if(c)
  {
    dbus_hold (c);
  }
  
  
}

/**
 * Make a call
 */
static void 
place_call( GtkWidget *widget, gpointer   data )
{
  call_t * c = (call_t*) call_list_get_by_state (CALL_STATE_DIALING);
  if(c)
  {
    sflphone_place_call(c);
  }
}

/**
 * Hang up the line
 */
static void 
hang_up( GtkWidget *widget, gpointer   data )
{
  call_t * c = (call_t*) call_list_get_by_state (CALL_STATE_CURRENT);
  if(c)
  {
    dbus_hang_up(c);
  }
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

/* Call back when the user click on a call in the list */
static void 
selected(GtkTreeSelection *sel, GtkTreeModel *model) 
{
	GtkTreeIter  iter;
	GValue val;
	
	if (! gtk_tree_selection_get_selected (sel, &model, &iter))
		return;
  
	val.g_type = G_TYPE_POINTER;
	gtk_tree_model_get_value (model, &iter, 2, &val);
	
	selectedCall = (call_t*) g_value_get_pointer(&val);
  g_value_unset(&val);
	
	if(selectedCall)
	{
	  if( selectedCall->state == CALL_STATE_INCOMING)
	  {
	    gtk_widget_set_sensitive( GTK_WIDGET(acceptButton),  TRUE);
      gtk_widget_set_sensitive( GTK_WIDGET(refuseButton),  TRUE);
      gtk_widget_set_sensitive( GTK_WIDGET(unholdButton),  FALSE);
      
    }
    else if( selectedCall->state == CALL_STATE_HOLD)
	  {
	    gtk_widget_set_sensitive( GTK_WIDGET(acceptButton),  FALSE);
      gtk_widget_set_sensitive( GTK_WIDGET(refuseButton),  FALSE);
      gtk_widget_set_sensitive( GTK_WIDGET(unholdButton),  TRUE);
    }
    else if(selectedCall->state == CALL_STATE_DIALING)
    {
      /*gtk_widget_hide( hangupButton );
      gtk_widget_show( callButton );
      gtk_widget_set_sensitive( GTK_WIDGET(callButton),       TRUE);
      gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     FALSE);
      gtk_widget_set_sensitive( GTK_WIDGET(holdButton),       FALSE);
      gtk_widget_set_sensitive( GTK_WIDGET(transfertButton),  FALSE);*/
    }
    else if (selectedCall->state == CALL_STATE_CURRENT)
    {
      //gtk_widget_hide( callButton  );
      /* Hack : if hangupButton is put on the window in create_screen()
       * the hbox will request space for 4 buttons making the window larger than needed */
      //gtk_box_pack_start (GTK_BOX (hbox), hangupButton, TRUE /*expand*/, TRUE /*fill*/, 10 /*padding*/);
      //gtk_box_reorder_child(GTK_BOX (hbox), hangupButton, 0);
      gtk_widget_show( hangupButton );
      gtk_widget_set_sensitive( GTK_WIDGET(callButton),       FALSE);
      gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     TRUE);
      gtk_widget_set_sensitive( GTK_WIDGET(holdButton),       TRUE);
      gtk_widget_set_sensitive( GTK_WIDGET(transfertButton),  TRUE);
    }
	  
	}
  
}

/**
 * Accept incoming call
 */
static void 
accept( GtkWidget *widget, gpointer   data )
{
  if(selectedCall)
  {
    dbus_accept(selectedCall);
  }
}

/**
 * Refuse incoming call
 */
static void 
refuse( GtkWidget *widget, gpointer   data )
{
  if(selectedCall)
  {
    dbus_refuse(selectedCall);
  }
}

/**
 * Unhold call
 */
static void 
unhold( GtkWidget *widget, gpointer   data )
{
  if(selectedCall)
  {
    dbus_unhold(selectedCall);
  }
}

GtkWidget * 
create_call_tree (){
  GtkWidget *ret;
	GtkWidget *sw;
  GtkWidget *hbox;
	GtkWidget *view;
	GtkWidget *image;
	GtkWidget *bbox;
	GtkCellRenderer *rend;
	GtkTreeViewColumn *col;
	GtkTreeSelection *sel;
	GtkTargetEntry te[2] = {{"text/uri-list", 0, 1},{"STRING", 0, 2}};

	ret = gtk_vbox_new(FALSE, 10); 
	gtk_container_set_border_width (GTK_CONTAINER (ret), 0);

	sw = gtk_scrolled_window_new( NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);

	gtk_box_pack_start(GTK_BOX(ret), sw, TRUE, TRUE, 0);
	store = gtk_list_store_new (3, 
	  GDK_TYPE_PIXBUF,// Icon 
	  G_TYPE_STRING,  // Description
	  G_TYPE_POINTER  // Pointer to the Object
	  );


	view = gtk_tree_view_new_with_model (GTK_TREE_MODEL(store));
  gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(view), FALSE);
	
	/*g_signal_connect(G_OBJECT(rend), "toggled",
							 G_CALLBACK(module_toggled), module_store);*/

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
  hbox = gtk_hbutton_box_new ();       
  gtk_button_box_set_spacing ( hbox, 10);
  gtk_button_box_set_layout ( GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_START);
  
  acceptButton = gtk_button_new_with_label ("Accept");
  image = gtk_image_new_from_file( PIXMAPS_DIR "/accept.svg");
  gtk_image_set_pixel_size(GTK_IMAGE(image), 16);
  gtk_button_set_image(GTK_BUTTON(acceptButton), image);
  gtk_widget_set_state( GTK_WIDGET(acceptButton), GTK_STATE_INSENSITIVE);
  gtk_box_pack_start (GTK_BOX (hbox), acceptButton, TRUE /*expand*/, TRUE /*fill*/, 10 /*padding*/);
  g_signal_connect (G_OBJECT (acceptButton), "clicked",
                    G_CALLBACK (accept), NULL);

  refuseButton = gtk_button_new_with_label ("Refuse");
  image = gtk_image_new_from_file( PIXMAPS_DIR "/refuse.svg");
  gtk_button_set_image(GTK_BUTTON(refuseButton), image);
  gtk_widget_set_state( GTK_WIDGET(refuseButton), GTK_STATE_INSENSITIVE);
  gtk_box_pack_start (GTK_BOX (hbox), refuseButton, TRUE /*expand*/, TRUE /*fill*/, 10 /*padding*/);
  g_signal_connect (G_OBJECT (refuseButton), "clicked",
                    G_CALLBACK (refuse), NULL);

  unholdButton = gtk_button_new_with_label ("Unhold");
  image = gtk_image_new_from_file( PIXMAPS_DIR "/unhold.svg");
  gtk_button_set_image(GTK_BUTTON(unholdButton), image);
  gtk_widget_set_state( GTK_WIDGET(unholdButton), GTK_STATE_INSENSITIVE);
  gtk_box_pack_start (GTK_BOX (hbox), unholdButton, TRUE /*expand*/, TRUE /*fill*/, 10 /*padding*/);
  g_signal_connect (G_OBJECT (unholdButton), "clicked",
                    G_CALLBACK (unhold), NULL);
                    
  gtk_box_pack_start (GTK_BOX (ret), hbox, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
  
  /* 2nd row */     
  hbox = gtk_hbutton_box_new ();       
  gtk_button_box_set_spacing ( GTK_BUTTON_BOX(hbox), 10);
  gtk_button_box_set_layout ( GTK_BUTTON_BOX(hbox), GTK_BUTTONBOX_START); 

  callButton = gtk_button_new_with_label ("Call");
  gtk_widget_set_state( GTK_WIDGET(callButton), GTK_STATE_INSENSITIVE);
  image = gtk_image_new_from_file( PIXMAPS_DIR "/call.svg");
  gtk_button_set_image(GTK_BUTTON(callButton), image);
  //gtk_button_set_image_position( button, GTK_POS_TOP);
  gtk_box_pack_start (GTK_BOX (hbox), callButton, TRUE /*expand*/, TRUE /*fill*/, 10 /*padding*/);
  g_signal_connect (G_OBJECT (callButton), "clicked",
                    G_CALLBACK (place_call), NULL);

  hangupButton = gtk_button_new_with_label ("Hang up");
  gtk_widget_hide( hangupButton );
  gtk_widget_set_state( GTK_WIDGET(hangupButton), GTK_STATE_INSENSITIVE);
  gtk_box_pack_end (GTK_BOX (hbox), hangupButton, TRUE /*expand*/, TRUE /*fill*/, 10 /*padding*/);
  image = gtk_image_new_from_file( PIXMAPS_DIR "/hang_up.svg");
  gtk_button_set_image(GTK_BUTTON(hangupButton), image);
  //gtk_button_set_image_position( button, GTK_POS_TOP);
  g_signal_connect (G_OBJECT (hangupButton), "clicked",
                    G_CALLBACK (hang_up), NULL);

  holdButton = gtk_button_new_with_label ("Hold");
  gtk_widget_set_state( GTK_WIDGET(holdButton), GTK_STATE_INSENSITIVE);
  image = gtk_image_new_from_file( PIXMAPS_DIR "/hold.svg");
  gtk_box_pack_end (GTK_BOX (hbox), holdButton, TRUE /*expand*/, TRUE /*fill*/, 10 /*padding*/);
  gtk_button_set_image(GTK_BUTTON(holdButton), image);
  g_signal_connect (G_OBJECT (holdButton), "clicked",
                    G_CALLBACK (hold), NULL);

  transfertButton = gtk_button_new_with_label ("Transfert");
  gtk_widget_set_state( GTK_WIDGET(transfertButton), GTK_STATE_INSENSITIVE);
  image = gtk_image_new_from_file( PIXMAPS_DIR "/transfert.svg");
  gtk_button_set_image(GTK_BUTTON(transfertButton), image);
  gtk_box_pack_end (GTK_BOX (hbox), transfertButton, TRUE /*expand*/, TRUE /*fill*/, 10 /*padding*/);
  g_signal_connect (G_OBJECT (transfertButton), "clicked",
                    G_CALLBACK (transfert), NULL);

  
  
  gtk_box_pack_start (GTK_BOX (ret), hbox, FALSE /*expand*/, TRUE /*fill*/, 0 /*padding*/);
 
	gtk_widget_show(ret); 
	
	return ret;
	
}

void 
update_call_tree ()
{
  GdkPixbuf *pixbuf;
	GtkTreeIter iter;

	gtk_list_store_clear(store);
  int i;
	for( i = 0; i < call_list_get_size(); i++)
	{
    call_t  * c = call_list_get_nth (i);
    if (c)
    {
      gchar * markup;
      if (c->state == CALL_STATE_CURRENT)
  		{
  		  markup = g_markup_printf_escaped("<big><b>%s</b></big>\n"
  						    "%s", 
  						    call_get_name(c), 
  						    call_get_number(c));
  		}
  		else 
  		{
  		  markup = g_markup_printf_escaped("<b>%s</b>\n"
  						    "%s", 
  						    call_get_name(c), 
  						    call_get_number(c));
  		}
  		
  		gtk_list_store_append (store, &iter);
  		
  		if (c->state == CALL_STATE_HOLD)
  		{
  		  pixbuf = gdk_pixbuf_new_from_file(PIXMAPS_DIR "/hold.svg", NULL);
  		}
  		else if (c->state == CALL_STATE_INCOMING)
  		{
  		  pixbuf = gdk_pixbuf_new_from_file(PIXMAPS_DIR "/ring.svg", NULL);
  		}
  	  else if (c->state == CALL_STATE_CURRENT)
  		{
  		  pixbuf = gdk_pixbuf_new_from_file(PIXMAPS_DIR "/current.svg", NULL);
  		}
  	  else if (c->state == CALL_STATE_DIALING)
  		{
  		  pixbuf = gdk_pixbuf_new_from_file(PIXMAPS_DIR "/dial.svg", NULL);
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
  		
  	}
    
  } 
  
  gtk_widget_set_sensitive( GTK_WIDGET(acceptButton),     FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(refuseButton),     FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(unholdButton),     FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(callButton),       FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(holdButton),       FALSE);
  //gtk_widget_set_sensitive( GTK_WIDGET(transfertButton),  FALSE);
	//return row_ref;
}
