/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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

#include <calltree.h>
#include <stdlib.h>
#include <glib/gprintf.h>
#include <calllist.h>
#include <conferencelist.h>
#include <mainwindow.h>
#include <history.h>

GtkWidget *sw;
GtkCellRenderer *rend;
GtkTreeViewColumn *col;
GtkTreeSelection *sel;

gint dragged_type;
gint selected_type;

gchar *dragged_call_id;
gchar *selected_call_id;

gchar *dragged_path;
gchar *selected_path;

gint dragged_path_depth;
gint selected_path_depth;

callable_obj_t *dragged_call;
callable_obj_t *selected_call;

conference_obj_t *dragged_conf;
conference_obj_t *selected_conf;


static void drag_begin_cb(GtkWidget *widget, GdkDragContext *dc, gpointer data);
static void drag_end_cb(GtkWidget * mblist, GdkDragContext * context, gpointer data);
void drag_data_received_cb(GtkWidget *widget, GdkDragContext *dc, gint x, gint y, GtkSelectionData *selection_data, guint info, guint t, gpointer data);


enum {
	COLUMN_ACCOUNT_STATE = 0,
	COLUMN_ACCOUNT_DESC,
	COLUMN_ACCOUNT_SECURITY,
	COLUMN_ACCOUNT_PTR,
};

/**
 * Show popup menu
 */
	static gboolean
popup_menu (GtkWidget *widget,
		gpointer   user_data UNUSED)
{
	show_popup_menu(widget, NULL);
	return TRUE;
}

/* Call back when the user click on a call in the list */
	static void
call_selected_cb(GtkTreeSelection *sel, void* data UNUSED )
{

    DEBUG("CallTree: Selection callback");

    GtkTreeIter iter;
    GValue val;
    GtkTreeModel *model = (GtkTreeModel*)active_calltree->store;
    
    GtkTreePath* path;
    gchar* string_path;


    if (! gtk_tree_selection_get_selected (sel, &model, &iter)) {
        return;
    }

    // store info for dragndrop
    path = gtk_tree_model_get_path(model, &iter);
    string_path = gtk_tree_path_to_string(path);
    selected_path_depth = gtk_tree_path_get_depth(path);

    if(gtk_tree_model_iter_has_child(GTK_TREE_MODEL(model), &iter)) {

        DEBUG("CallTree: Selected a conference");
	selected_type = A_CONFERENCE;

	val.g_type = 0;
	gtk_tree_model_get_value (model, &iter, COLUMN_ACCOUNT_PTR, &val);

	calltab_select_conf((conference_obj_t*) g_value_get_pointer(&val));

	selected_conf = (conference_obj_t*)g_value_get_pointer(&val);

	if(selected_conf) {

	    selected_call_id = selected_conf->_confID;
	    selected_path = string_path;
	    selected_call = NULL;

	}

	DEBUG("CallTree: selected_path %s, selected_call_id %s, selected_path_depth %d", 
	                          selected_path, selected_call_id, selected_path_depth);

    }
    else {
      
        DEBUG("CallTree: Selected a call");
	selected_type = A_CALL;

	val.g_type = 0;
	gtk_tree_model_get_value (model, &iter, COLUMN_ACCOUNT_PTR, &val);
	
	calltab_select_call(active_calltree, (callable_obj_t*) g_value_get_pointer(&val));
	
	selected_call = (callable_obj_t*)g_value_get_pointer(&val);

	if(selected_call) {

	    selected_call_id = selected_call->_callID;
	    selected_path = string_path;
	    selected_conf = NULL;
	}

	DEBUG("CallTree: selected_path %s, selected_call_id %s, selected_path_depth %d", 
	                            selected_path, selected_call_id, selected_path_depth);
    }

    g_value_unset(&val);
    update_actions();
}

/* A row is activated when it is double clicked */
	void 
row_activated(GtkTreeView       *tree_view UNUSED,
		GtkTreePath       *path UNUSED,
		GtkTreeViewColumn *column UNUSED,
		void * data UNUSED) {
    callable_obj_t* selectedCall = NULL;
    callable_obj_t* new_call;
    conference_obj_t* selectedConf = NULL;
    gchar *account_id;
	
    DEBUG("CallTree: Double click action");
    
    if(calltab_get_selected_type(active_calltree) == A_CALL) {
      
	selectedCall = calltab_get_selected_call(active_calltree);

	if (selectedCall) {
	    DEBUG("CallTree: Selected a call");
	    
	    // Get the right event from the right calltree
	    if( active_calltree == current_calls ) {

	      DEBUG("CallTree: Active tree is current calls");
	      
	      switch(selectedCall->_state) {
	      case CALL_STATE_INCOMING:
		  dbus_accept(selectedCall);
		  stop_notification();
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
		  WARN("Row activated - Should not happen!");
		  break;
	      }
	    }
		
	    // If history or contact: double click action places a new call
	    else {

	        DEBUG("CallTree: Active tree is history or contact");

		account_id = g_strdup (selectedCall->_accountID);
		      
		// Create a new call
		create_new_call (CALL, CALL_STATE_DIALING, "", account_id, selectedCall->_peer_name, selectedCall->_peer_number, &new_call);

		calllist_add(current_calls, new_call);
		calltree_add_call(current_calls, new_call, NULL);
		sflphone_place_call(new_call);
		calltree_display(current_calls);
	    }
	}
    }
    else if(calltab_get_selected_type(current_calls) == A_CONFERENCE) {

        DEBUG("CallTree: Selected a conference");
	    
	if( active_calltree == current_calls ) {

	    selectedConf = calltab_get_selected_conf(current_calls);

	    if(selectedConf) {

	        switch(selectedConf->_state) {
		case CONFERENCE_STATE_ACTIVE_ATACHED:
		    // sflphone_add_main_participant(selectedConf);
		    break;
		case CONFERENCE_STATE_ACTIVE_DETACHED:
		    sflphone_add_main_participant(selectedConf);
		    break;
		case CONFERENCE_STATE_HOLD:
		    sflphone_conference_off_hold(selectedConf);
		    break;
		}
	    }
	}
    }
}


/* Catch cursor-activated signal. That is, when the entry is single clicked */
	void  
row_single_click(GtkTreeView *tree_view UNUSED, void * data UNUSED)
{
    callable_obj_t * selectedCall = NULL;
    account_t * account_details = NULL;
    gchar * displaySasOnce="";

    DEBUG("CallTree: Single click action");

    selectedCall = calltab_get_selected_call( active_calltree );

    /*
    if(!selected_call) {
      selected_call = selectedCall;
    }
    */

    if (selectedCall) {

        account_details = account_list_get_by_id(selectedCall->_accountID);
	DEBUG("AccountID %s", selectedCall->_accountID);

	if(account_details != NULL) {
	    displaySasOnce = g_hash_table_lookup(account_details->properties, ACCOUNT_DISPLAY_SAS_ONCE);
	    DEBUG("Display SAS once %s", displaySasOnce);
	}
	else {
	    GHashTable * properties = NULL;
	    sflphone_get_ip2ip_properties (&properties);
	    if(properties != NULL)
	      { displaySasOnce = g_hash_table_lookup(properties, ACCOUNT_DISPLAY_SAS_ONCE); DEBUG("IP2IP displaysasonce %s", displaySasOnce); }
	}

	/*  Make sure that we are not in the history tab since 
	 *  nothing is defined for it yet 
	 */
	if( active_calltree == current_calls ) {

	    // sflphone_selected_call_codec(selectedCall);

	    // DEBUG("single click action: %s", dbus_get_current_codec_name(selectedCall));
	    // sflphone_display_selected_codec(dbus_get_current_codec_name(selectedCall));

	  switch(selectedCall->_srtp_state) {

	  case SRTP_STATE_ZRTP_SAS_UNCONFIRMED:
	      selectedCall->_srtp_state = SRTP_STATE_ZRTP_SAS_CONFIRMED;
	      if(g_strcasecmp(displaySasOnce,"true") == 0) {
		  selectedCall->_zrtp_confirmed = TRUE;
	      }
	      dbus_confirm_sas(selectedCall);
	      calltree_update_call(current_calls, selectedCall, NULL);
	      break;
	  case SRTP_STATE_ZRTP_SAS_CONFIRMED:
	      selectedCall->_srtp_state = SRTP_STATE_ZRTP_SAS_UNCONFIRMED;
	      dbus_reset_sas(selectedCall);
	      calltree_update_call(current_calls, selectedCall, NULL);
	      break;
	  default:
	      DEBUG("Single click but no action");
	      break;
	  }
	}
    }
}

static gboolean
button_pressed(GtkWidget* widget, GdkEventButton *event, gpointer user_data UNUSED)
{
    if (event->button == 3 && event->type == GDK_BUTTON_PRESS){
        if( active_calltree == current_calls ) {
	    show_popup_menu(widget,  event);
	    return TRUE;
	}
	else if (active_calltree == history) {
	    show_popup_menu_history (widget,  event);
	    return TRUE;
	}
	else {
	    show_popup_menu_contacts (widget, event);
	    return TRUE;
	}
    }
    return FALSE;
}


gchar* 
calltree_display_call_info(callable_obj_t * c, CallDisplayType display_type, gchar *audio_codec, gchar** display_info)
{

    gchar * description;
    gchar * tmp_info;

    gchar * peer_number = c->_peer_number;
    gchar * hostname = NULL;
    gchar * display_number = "";  

    DEBUG("CallTree: Display call info");

    // If call is outgoing, keep the hostname, strip it elsewhere
    if(c->_type == CALL && c->_history_state == OUTGOING) {

        display_number = peer_number; 
    }
    else {

        // Get the hostname for this call (NULL if not existent)
        hostname = g_strrstr(peer_number, "@");

	// Test if we are dialing a new number
	if(g_strcmp0("", c->_peer_number) != 0) {

	    // Strip the hostname if existent
	    if(hostname) {
	        display_number = g_strndup(peer_number, hostname - peer_number);
	    }
	    else {
	        display_number = peer_number;
	    }
	}
	else {

	    display_number = peer_number;
	}
    }
    // Different display depending on type
    switch(display_type) {

    case DISPLAY_TYPE_CALL:

        DEBUG("CallTree: Display a normal call");
        if(c->_state_code == 0) {

	    if(g_strcmp0("", c->_peer_name) == 0) {
	        description = g_markup_printf_escaped("<b>%s</b><i>%s</i>",
						      display_number, c->_peer_name);
	    }
	    else {
	        description = g_markup_printf_escaped("<b>%s</b>   <i>%s</i>",
						      c->_peer_name, display_number);
	    }

	}
	else {
	    if(g_strcmp0("", c->_peer_name) == 0) {
	        description = g_markup_printf_escaped("<b>%s</b><i>%s</i>\n<i>%s (%d)</i>",
						      display_number, c->_peer_name,
						      c->_state_code_description, c->_state_code);
	    }
	    else {
	        description = g_markup_printf_escaped("<b>%s</b>   <i>%s</i>\n<i>%s (%d)</i>",
						      c->_peer_name, display_number,
						      c->_state_code_description, c->_state_code);
	    }
	}
	break;


    case DISPLAY_TYPE_CALL_TRANSFER: 

        DEBUG("CallTree: Display a call transfer");

        if(g_strcmp0("",c->_peer_name) == 0){
	    description = g_markup_printf_escaped("<b>%s</b><i>%s</i>\n<i>Transfert to:%s</i> ",
						  display_number, c->_peer_name, c->_trsft_to);
	}
	else {
	    description = g_markup_printf_escaped("<b>%s</b>   <i>%s</i>\n<i>Transfert to:%s</i> ",
						  c->_peer_name, display_number, c->_trsft_to);
	}	
	break;


    case DISPLAY_TYPE_STATE_CODE : 

        DEBUG("CallTree: Display a state code");

        if(g_strcmp0("",c->_peer_name) == 0){

	    if (c->_state_code) {

	        description = g_markup_printf_escaped("<b>%s</b><i>%s</i>\n<i>%s (%d)</i>  <i>%s</i>",
						      display_number, c->_peer_name,
						      c->_state_code_description, c->_state_code,
						      audio_codec);
	    } else {
	        description = g_markup_printf_escaped("<b>%s</b><i>%s</i>\n<i>%s</i>",
						      display_number, c->_peer_name, audio_codec);
	    }
	}
	else {
	    if (c->_state_code) {
	        description = g_markup_printf_escaped("<b>%s</b>   <i>%s</i>\n<i>%s (%d)</i>  <i>%s</i>",
						      c->_peer_name, display_number, 
						      c->_state_code_description, c->_state_code,
						      audio_codec);
	    } else {
	        description = g_markup_printf_escaped("<b>%s</b>   <i>%s</i>\n<i>%s</i>",
						      c->_peer_name, display_number, audio_codec);
	    }
	}
	break;

    case DISPLAY_TYPE_SAS:

        DEBUG("CallTree: Display a call with sas");

        if(g_strcmp0("", c->_peer_name) == 0){
	    description = g_markup_printf_escaped("<b>%s</b><i>%s</i>\n<i>Confirm SAS <b>%s</b> ?</i> ",
						  display_number, c->_peer_name, c->_sas);
	}
	else {
	  description = g_markup_printf_escaped("<b>%s</b>   <i>%s</i>\n<i>Confirm SAS <b>%s</b> ?</i> ",
						c->_peer_name, display_number, c->_sas);
	}
	break;

    case DISPLAY_TYPE_HISTORY :

        DEBUG("CallTree: Display history entry");

        if(g_strcmp0("", c->_peer_name) == 0) {
	    description = g_markup_printf_escaped("<b>%s</b><i>%s</i>",
						  display_number, c->_peer_name);
	}
	else {
	  description = g_markup_printf_escaped("<b>%s</b>   <i>%s</i>",
						c->_peer_name, display_number);
	}
	break;

    default : 
        DEBUG("CallTree: Not an allowable type of display");
	break;

    }

    // return description;
    tmp_info = g_strdup(description);
    *display_info = tmp_info;
}



/**
 * Reset call tree
 */
	void
calltree_reset (calltab_t* tab)
{
	gtk_tree_store_clear (tab->store);
}

void
focus_on_calltree_out(){
	//DEBUG("set_focus_on_calltree_out");
	// gtk_widget_grab_focus(GTK_WIDGET(sw));
	focus_is_on_calltree = FALSE;
}

void
focus_on_calltree_in(){
	//DEBUG("set_focus_on_calltree_in");
	// gtk_widget_grab_focus(GTK_WIDGET(sw));
	focus_is_on_calltree = TRUE;
}

	void
calltree_create (calltab_t* tab, gboolean searchbar_type)
{

	tab->tree = gtk_vbox_new(FALSE, 10);

	// Fix bug #708 (resize)
	gtk_widget_set_size_request(tab->tree,100,80);

	gtk_container_set_border_width (GTK_CONTAINER (tab->tree), 0);

	sw = gtk_scrolled_window_new( NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
	gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);


	tab->store = gtk_tree_store_new (4,
			GDK_TYPE_PIXBUF,// Icon
			G_TYPE_STRING,  // Description
			GDK_TYPE_PIXBUF, // Security Icon
			G_TYPE_POINTER  // Pointer to the Object
			);

	tab->view = gtk_tree_view_new_with_model (GTK_TREE_MODEL(tab->store));
	gtk_tree_view_set_enable_search( GTK_TREE_VIEW(tab->view), FALSE);
	gtk_tree_view_set_headers_visible (GTK_TREE_VIEW(tab->view), FALSE);
	g_signal_connect (G_OBJECT (tab->view), "row-activated",
			G_CALLBACK (row_activated),
			NULL);

	GTK_WIDGET_SET_FLAGS (GTK_WIDGET(sw),GTK_CAN_FOCUS);
	gtk_widget_grab_focus (GTK_WIDGET(sw));

	g_signal_connect (G_OBJECT (tab->view), "cursor-changed",
			G_CALLBACK (row_single_click),
			NULL);

	// Connect the popup menu
	g_signal_connect (G_OBJECT (tab->view), "popup-menu",
			G_CALLBACK (popup_menu),
			NULL);
	g_signal_connect (G_OBJECT (tab->view), "button-press-event",
			G_CALLBACK (button_pressed),
			NULL);

	// g_signal_connect (G_OBJECT (sw), "key-release-event",
	//                   G_CALLBACK (on_key_released), NULL);

	g_signal_connect_after (G_OBJECT (tab->view), "focus-in-event",
			G_CALLBACK (focus_on_calltree_in), NULL);
	g_signal_connect_after (G_OBJECT (tab->view), "focus-out-event",
			G_CALLBACK (focus_on_calltree_out), NULL);


	if(tab != history && tab!=contacts) {

		DEBUG("SET TREE VIEW REORDABLE");
		// Make calltree reordable for drag n drop
		gtk_tree_view_set_reorderable(GTK_TREE_VIEW(tab->view), TRUE); 

		// source widget drag n drop signals
		g_signal_connect (G_OBJECT (tab->view), "drag_begin", G_CALLBACK (drag_begin_cb), NULL);
		g_signal_connect (G_OBJECT (tab->view), "drag_end", G_CALLBACK (drag_end_cb), NULL);

		// destination widget drag n drop signals
		g_signal_connect (G_OBJECT (tab->view), "drag_data_received", G_CALLBACK (drag_data_received_cb), NULL);

	}


	gtk_widget_grab_focus(GTK_WIDGET(tab->view));

	rend = gtk_cell_renderer_pixbuf_new();
	col = gtk_tree_view_column_new_with_attributes ("Icon",
			rend,
			"pixbuf", 0,
			NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(tab->view), col);

	rend = gtk_cell_renderer_text_new();
	col = gtk_tree_view_column_new_with_attributes ("Description",
			rend,
			"markup", COLUMN_ACCOUNT_DESC,
			NULL);
	g_object_set(rend, "wrap-mode", (PangoWrapMode) PANGO_WRAP_WORD_CHAR, NULL);
	g_object_set(rend, "wrap-width", (gint) CALLTREE_TEXT_WIDTH, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(tab->view), col);

	/* Security icon */
	rend = gtk_cell_renderer_pixbuf_new();
	col = gtk_tree_view_column_new_with_attributes ("Icon",
			rend,
			"pixbuf", COLUMN_ACCOUNT_SECURITY,
			NULL);
	g_object_set(rend, "xalign", (gfloat) 1.0, NULL);
	g_object_set(rend, "yalign", (gfloat) 0.0, NULL);
	gtk_tree_view_append_column (GTK_TREE_VIEW(tab->view), col);


	g_object_unref(G_OBJECT(tab->store));
	gtk_container_add(GTK_CONTAINER(sw), tab->view);

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (tab->view));
	g_signal_connect (G_OBJECT (sel), "changed",
			G_CALLBACK (call_selected_cb),
			NULL);

	gtk_box_pack_start(GTK_BOX(tab->tree), sw, TRUE, TRUE, 0);

	// search bar if tab is either "history" or "addressbook"
	if(searchbar_type){
		calltab_create_searchbar (tab);
		gtk_box_pack_start(GTK_BOX(tab->tree), tab->searchbar, FALSE, TRUE, 0);
	}

	gtk_widget_show(tab->tree);
}


	void
calltree_remove_call (calltab_t* tab, callable_obj_t * c, GtkTreeIter *parent)
{

	GtkTreeIter iter;
	GValue val;
	callable_obj_t * iterCall;
	GtkTreeStore* store = tab->store;

	if(!c)
	  ERROR("CallTree: Error: Not a valid call");

	DEBUG("CallTree: Remove call %s", c->_callID);

	int nbChild = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), parent);
	int i;
	for( i = 0; i < nbChild; i++)
	{
		if(gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, parent, i))
		{
			if(gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), &iter))
			{
				calltree_remove_call (tab, c, &iter);
			}

			val.g_type = 0;
			gtk_tree_model_get_value (GTK_TREE_MODEL(store), &iter, COLUMN_ACCOUNT_PTR, &val);

			iterCall = (callable_obj_t*) g_value_get_pointer(&val);
			g_value_unset(&val);

			if(iterCall == c)
			{
				gtk_tree_store_remove(store, &iter);
			}
		}
	}
	callable_obj_t * selectedCall = calltab_get_selected_call(tab);
	if(selectedCall == c)
		calltab_select_call(tab, NULL);
	update_actions();

	calltree_update_clock();

	DEBUG("Calltre remove call ended");
}

void
calltree_update_call (calltab_t* tab, callable_obj_t * c, GtkTreeIter *parent)
{
    GdkPixbuf *pixbuf=NULL;
    GdkPixbuf *pixbuf_security=NULL;
    GtkTreeIter iter;
    GValue val;
    callable_obj_t * iterCall;
    GtkTreeStore* store = tab->store;

    gchar* srtp_enabled = "";
    gboolean display_sas = TRUE;
    gboolean sdes_success = TRUE;
    account_t* account_details=NULL;
    gchar *audio_codec = "";

    int nbChild = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), parent);
    int i;

    if(c != NULL) {
        account_details = account_list_get_by_id(c->_accountID);
	if(account_details != NULL) {
	    srtp_enabled = g_hash_table_lookup(account_details->properties, ACCOUNT_SRTP_ENABLED);
	    if(g_strcasecmp(g_hash_table_lookup(account_details->properties, ACCOUNT_ZRTP_DISPLAY_SAS),"false") == 0) 
	      { display_sas = FALSE; }
	} else {
	    GHashTable * properties = NULL;
	    sflphone_get_ip2ip_properties (&properties);
	    if(properties != NULL) {
	        srtp_enabled = g_hash_table_lookup(properties, ACCOUNT_SRTP_ENABLED);
	        if(g_strcasecmp(g_hash_table_lookup(properties, ACCOUNT_ZRTP_DISPLAY_SAS),"false") == 0) 
		  { display_sas = FALSE; }
	    }
	}
    } 

    for( i = 0; i < nbChild; i++) {

        if(gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, parent, i)) {

	    if(gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), &iter)) {
	      calltree_update_call (tab, c, &iter);
	    }

	    val.g_type = 0;
	    gtk_tree_model_get_value (GTK_TREE_MODEL(store), &iter, COLUMN_ACCOUNT_PTR, &val);

	
	    iterCall = (callable_obj_t*) g_value_get_pointer(&val);
	    g_value_unset(&val);
	
	    if(iterCall != c) {
	        continue;
	    }

	    /* Update text */
	    gchar * description;
	    gchar * date="";
	    gchar * duration="";
	    audio_codec = call_get_audio_codec (c);
	
	    if(c->_state == CALL_STATE_TRANSFERT) {

	        calltree_display_call_info(c, DISPLAY_TYPE_CALL_TRANSFER, NULL, &description);
			    
	    }
	    else {

	        if((c->_sas != NULL) && (display_sas == TRUE) && (c->_srtp_state == SRTP_STATE_ZRTP_SAS_UNCONFIRMED) && (c->_zrtp_confirmed == FALSE)) {
		    calltree_display_call_info(c, DISPLAY_TYPE_SAS, NULL, &description);
		} else {
		    calltree_display_call_info(c, DISPLAY_TYPE_STATE_CODE, audio_codec, &description);		    
		}
	    }

	    /* Update icons */
	    if( tab == current_calls ) {
	        DEBUG("Receiving in state %d", c->_state);
		switch(c->_state) {
		case CALL_STATE_HOLD:
		    pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/hold.svg", NULL);
		    break;
		case CALL_STATE_INCOMING:
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
		case CALL_STATE_RECORD:
	            pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/icon_rec.svg", NULL);
		    break;
		default:
		    WARN("Update calltree - Should not happen!");
		}        

		switch(c->_srtp_state) {
		case SRTP_STATE_SDES_SUCCESS:
		    DEBUG("SDES negotiation succes");
		    pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_confirmed.svg", NULL); 
		    break;
		case SRTP_STATE_ZRTP_SAS_UNCONFIRMED:
	            DEBUG("Secure is ON");
		    pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_unconfirmed.svg", NULL);
		    if(c->_sas != NULL) { DEBUG("SAS is ready with value %s", c->_sas); }
		    break;
		case SRTP_STATE_ZRTP_SAS_CONFIRMED:
		    DEBUG("SAS is confirmed");
		    pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_confirmed.svg", NULL);   
		    break;
		case SRTP_STATE_ZRTP_SAS_SIGNED:   
		    DEBUG("Secure is ON with SAS signed and verified");
		    pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_certified.svg", NULL);
		    break;
		case SRTP_STATE_UNLOCKED:  
		    DEBUG("Secure is off calltree %d", c->_state);
		    if(g_strcasecmp(srtp_enabled,"true") == 0) {
		        pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_off.svg", NULL); 
		    }
		    break;
		default:
		    WARN("Update calltree srtp state #%d- Should not happen!", c->_srtp_state); 
		    if(g_strcasecmp(srtp_enabled,"true") == 0) {
		        pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_off.svg", NULL);    
		    }

		}
	
	    }
	    else if(tab == history) {
	        switch(c->_history_state) {
		case INCOMING:
		    pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/incoming.svg", NULL);
		    break;
		case OUTGOING:
		    pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/outgoing.svg", NULL);
		    break;
		case MISSED:
		    pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/missed.svg", NULL);
		    break;
		default:
		    WARN("History - Should not happen!");
		}

		calltree_display_call_info(c, DISPLAY_TYPE_HISTORY, NULL, &description);

		date = get_formatted_start_timestamp (c);
		duration = get_call_duration (c);
		duration = g_strconcat( date , duration , NULL);
		description = g_strconcat( description , duration, NULL);
	    }

	    gtk_tree_store_set(store, &iter,
			   0, pixbuf, // Icon
			   1, description, // Description
			   2, pixbuf_security,
			   3, c,
			   -1);

	    if (pixbuf != NULL)
	        g_object_unref(G_OBJECT(pixbuf));
	}
      
    }

    update_actions();
}


void calltree_add_call (calltab_t* tab, callable_obj_t * c, GtkTreeIter *parent)
{

        DEBUG("CallTree: Add call to calltree id: %s, peer name: %s", c->_callID, c->_peer_name);

	if (tab == history)
	{
		calltree_add_history_entry (c);
		return;
	}

	account_t* account_details=NULL;

	GdkPixbuf *pixbuf=NULL;
	GdkPixbuf *pixbuf_security=NULL;
	GtkTreeIter iter;
	gchar* key_exchange="";
	gchar* srtp_enabled="";

	// New call in the list
	gchar * description;
	gchar * date="";
	gchar *duration="";

	calltree_display_call_info(c, DISPLAY_TYPE_CALL, NULL, &description);

	gtk_tree_store_prepend (tab->store, &iter, parent);

	if(c != NULL) {
		account_details = account_list_get_by_id(c->_callID);
		if(account_details != NULL) {
			srtp_enabled = g_hash_table_lookup(account_details->properties, ACCOUNT_SRTP_ENABLED);
			key_exchange = g_hash_table_lookup(account_details->properties, ACCOUNT_KEY_EXCHANGE);
		}
	} 

	DEBUG("Added call key exchange is %s", key_exchange);

	if( tab == current_calls )
	{
		switch(c->_state)
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
			case CALL_STATE_CURRENT:
				// If the call has been initiated by a another client and, when we start, it is already current
				pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/current.svg", NULL);
				break;
			case CALL_STATE_HOLD:
				// If the call has been initiated by a another client and, when we start, it is already current
				pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/hold.svg", NULL);
				break;
			case CALL_STATE_FAILURE:
				// If the call has been initiated by a another client and, when we start, it is already current
				pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/fail.svg", NULL);
				break;
			default:
				WARN("Update calltree add - Should not happen!");
		}

		if(g_strcasecmp(srtp_enabled,"true") == 0) {
			pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/secure_off.svg", NULL);
		}
	}

	else if (tab == contacts) {
		pixbuf = c->_contact_thumbnail;
		description = g_strconcat( description , NULL);
	}

	else {
		WARN ("This widget doesn't exist - This is a bug in the application.");
	}


	//Resize it
	if(pixbuf)
	{
		if(gdk_pixbuf_get_width(pixbuf) > 32 || gdk_pixbuf_get_height(pixbuf) > 32)
		{
			pixbuf =  gdk_pixbuf_scale_simple(pixbuf, 32, 32, GDK_INTERP_BILINEAR);
		}
	}

	if(pixbuf_security)
	{
		if(gdk_pixbuf_get_width(pixbuf_security) > 32 || gdk_pixbuf_get_height(pixbuf_security) > 32)
		{
			pixbuf_security =  gdk_pixbuf_scale_simple(pixbuf_security, 32, 32, GDK_INTERP_BILINEAR);
		}
	}

	gtk_tree_store_set(tab->store, &iter,
			0, pixbuf, // Icon
			1, description, // Description
			2, pixbuf_security, // Informs user about the state of security
			3, c,      // Pointer
			-1);

	if (pixbuf != NULL)
	{ g_object_unref(G_OBJECT(pixbuf)); }
	if (pixbuf_security != NULL)
	{ g_object_unref(G_OBJECT(pixbuf)); }

	gtk_tree_view_set_model (GTK_TREE_VIEW(history->view), GTK_TREE_MODEL(history->store));

	gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(tab->view)), &iter);

	history_reinit(history);
}

void calltree_add_history_entry (callable_obj_t * c)
{

	DEBUG("calltree_add_history_entry %s", c->_callID);

	if (!eel_gconf_get_integer (HISTORY_ENABLED))
		return;

	GdkPixbuf *pixbuf=NULL;
	GdkPixbuf *pixbuf_security=NULL;
	GtkTreeIter iter;

	// New call in the list
	gchar * description, *date="", *duration="";

	calltree_display_call_info(c, DISPLAY_TYPE_HISTORY, NULL, &description);

	gtk_tree_store_prepend (history->store, &iter, NULL);

	switch(c->_history_state)
	{
		case INCOMING:
			pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/incoming.svg", NULL);
			break;
		case OUTGOING:
			pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/outgoing.svg", NULL);
			break;
		case MISSED:
			pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/missed.svg", NULL);
			break;
		default:
			WARN("History - Should not happen!");
	}

	date = get_formatted_start_timestamp (c);
	duration = get_call_duration (c);
	duration = g_strconcat( date , duration , NULL);
	description = g_strconcat( description , duration, NULL);

	//Resize it
	if(pixbuf)
	{
		if(gdk_pixbuf_get_width(pixbuf) > 32 || gdk_pixbuf_get_height(pixbuf) > 32)
		{
			pixbuf =  gdk_pixbuf_scale_simple(pixbuf, 32, 32, GDK_INTERP_BILINEAR);
		}
	}

	if(pixbuf_security != NULL)
	{
		if(gdk_pixbuf_get_width(pixbuf_security) > 32 || gdk_pixbuf_get_height(pixbuf_security) > 32)
		{
			pixbuf_security =  gdk_pixbuf_scale_simple(pixbuf_security, 32, 32, GDK_INTERP_BILINEAR);
		}
	}
	gtk_tree_store_set(history->store, &iter,
			0, pixbuf, // Icon
			1, description, // Description
			2, pixbuf_security, // Icon
			3, c,      // Pointer
			-1);

	if (pixbuf != NULL)
		g_object_unref(G_OBJECT(pixbuf));
	if (pixbuf_security != NULL)
	{ g_object_unref(G_OBJECT(pixbuf_security)); }

	gtk_tree_view_set_model (GTK_TREE_VIEW(history->view), GTK_TREE_MODEL(history->store));

	history_reinit(history);
}


void calltree_add_conference (calltab_t* tab, conference_obj_t* conf)
{

    GdkPixbuf *pixbuf=NULL;
    GdkPixbuf *pixbuf_security=NULL;
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeModel *model = (GtkTreeModel*)active_calltree->store;

    // gchar** participant = dbus_get_participant_list(conf->_confID);
    // gchar** pl;
    GSList* conference_participant;
    gchar* call_id;

    callable_obj_t * call;

    account_t* account_details=NULL;
    gchar* srtp_enabled="";

    // New call in the list
    
    gchar * description;


    if(!conf) {
      ERROR("Calltree: Error: Conference is null!!");
      return;
    }

    DEBUG("Calltree: Add conference %s", conf->_confID);

    // description = g_markup_printf_escaped("<b>%s</b>", conf->_confID);
    description = g_markup_printf_escaped("<b>%s</b>", "");

    gtk_tree_store_append (tab->store, &iter, NULL);

    if( tab == current_calls )
    {	

        switch(conf->_state)
	{
	    case CONFERENCE_STATE_ACTIVE_ATACHED:
	    {
			
	        pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/usersActive.svg", NULL);
		break;
	    }
	    case CONFERENCE_STATE_ACTIVE_DETACHED:
	    case CONFERENCE_STATE_HOLD:
	    {
			
			
	        pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/users.svg", NULL);
		break;
	    }
	    default:
	        WARN("Update conference add - Should not happen!");
	}
    }
    else
    {
	DEBUG("Error Conference State NULL for conferece %s!!!!!", conf->_confID);
    }


    //Resize it
    if(pixbuf) {
	if(gdk_pixbuf_get_width(pixbuf) > 32 || gdk_pixbuf_get_height(pixbuf) > 32) {
	    pixbuf =  gdk_pixbuf_scale_simple(pixbuf, 32, 32, GDK_INTERP_BILINEAR);
	}
    }
    else {
        DEBUG("Error no pixbuff for conference from %s", ICONS_DIR);
    }
    
    // Used to determine if at least one participant use a security feature
    // If true (at least on call use a security feature) we need to display security icons 
    conf->_conf_srtp_enabled = FALSE;
    
    // Used to determine if the conference is secured
    // Every participant to a conference must be secured, the conference is not secured elsewhere
    conf->_conference_secured = TRUE;

    DEBUG("Calltree: Determine if conference is secured");
    
    // participant = conf->participant;
    // participant = dbus_get_participant_list(conf->_confID);
    conference_participant = conf->participant_list;
    if(conference_participant) {

        DEBUG("Calltree: Determine if at least one participant uses SRTP");
	
	// participant = conf->participant;
	// participant = dbus_get_participant_list(conf->_confID);
	// for (pl = participant; *pl; pl++)
	while(conference_participant) {
	    
	    // call_id = (gchar*)(*pl);
	    call_id = (gchar*)(conference_participant->data);
	    call = calllist_get (tab, call_id);
	    
	    if(call != NULL) {
	      
	      account_details = account_list_get_by_id(call->_callID);
	      if(account_details != NULL) {
		srtp_enabled = g_hash_table_lookup(account_details->properties, ACCOUNT_SRTP_ENABLED);
	      }
	      
	      if(g_strcasecmp(srtp_enabled,"true") == 0) {
		DEBUG("Calltree: SRTP enabled for participant %s", call_id);
		conf->_conf_srtp_enabled = TRUE;
		break;
	      } 
	      else {
		DEBUG("Calltree: SRTP is not enabled for participant %s", call_id);
	      }
	      
	    }
	    
	    conference_participant = conference_next_participant(conference_participant);
	    
	  }

	DEBUG("Calltree: Determine if all conference participant are secured");
	
	if(conf->_conf_srtp_enabled) {
	    // participant = conf->participant;
	    conference_participant = conf->participant_list;
	    // for (pl = participant; *pl; pl++)
	    while(conference_participant) {	    
		// call_id = (gchar*)(*pl);
		call_id = (gchar*)(conference_participant->data);
		call = calllist_get (tab, call_id);
		
		if(call != NULL) {
		  
		  if(call->_srtp_state == 0) {
		      DEBUG("Calltree: Participant %s is not secured", call_id);
							conf->_conference_secured = FALSE;
							break;
		  }
		  else {
		      DEBUG("Calltree: Participant %s is secured", call_id);
		  }
		}
		conference_participant = conference_next_participant(conference_participant);
	    }
	}
    }

    if(conf->_conf_srtp_enabled) {
	if(conf->_conference_secured) {
	    DEBUG("Calltree: Conference is secured");
	    pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_confirmed.svg", NULL);
	}
	else {
	    DEBUG("Calltree: Conference is not secured");
	    pixbuf_security = gdk_pixbuf_new_from_file(ICONS_DIR "/lock_off.svg", NULL);
	}
    }
    
    DEBUG("Calltree: Add conference to tree store");
    
    gtk_tree_store_set(tab->store, &iter,
		       0, pixbuf, // Icon
		       1, description, // Description
		       2, pixbuf_security,
		       3, conf, // Pointer
		       -1);

    if (pixbuf != NULL)
      g_object_unref(G_OBJECT(pixbuf));
    
    // participant = conf->participant;
    // participant = dbus_get_participant_list(conf->_confID);
    conference_participant = conf->participant_list;
    if(conference_participant) {

        DEBUG("Calltre: Adding conference participant");
	// for (pl = participant; *pl; pl++)
	while(conference_participant) {
	    
	    DEBUG("OK");
	    call_id = (gchar*)(conference_participant->data);
	    call = calllist_get (tab, call_id);
	    // create_new_call_from_details (conf_id, conference_details, &c);
	    
	    calltree_remove_call(tab, call, NULL);
	    calltree_add_call (tab, call, &iter);
	    
	    conference_participant = conference_next_participant(conference_participant);
	}
    }
    /*
    else 
	  {
	    WARN ("Conferences cannot be added in this widget - This is a bug in the application.");    
	  }
    */

    gtk_tree_view_set_model(GTK_TREE_VIEW(tab->view), GTK_TREE_MODEL(tab->store));

    path = gtk_tree_model_get_path(model, &iter);

    gtk_tree_view_expand_row(GTK_TREE_VIEW(tab->view), path, FALSE);

    update_actions();

}


void calltree_update_conference (calltab_t* tab, const gchar* confID)
{

	DEBUG("calltree_update_conference");


}


void calltree_remove_conference (calltab_t* tab, const conference_obj_t* conf, GtkTreeIter *parent)
{

    DEBUG("calltree_remove_conference %s\n", conf->_confID);
    
    GtkTreeIter iter_parent;
    GtkTreeIter iter_child;
    GValue confval;
    GValue callval;
    conference_obj_t * tempconf;
    callable_obj_t * call;
    GtkTreeStore* store = tab->store;

    int nbChild = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), parent);

    int nbParticipant;

    int i, j;
    for( i = 0; i < nbChild; i++) {

        if(gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter_parent, parent, i)) {

	    if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), &iter_parent)) {

	        calltree_remove_conference (tab, conf, &iter_parent);

		confval.g_type = 0;
		gtk_tree_model_get_value (GTK_TREE_MODEL(store), &iter_parent, COLUMN_ACCOUNT_PTR, &confval);

		tempconf = (conference_obj_t*) g_value_get_pointer(&confval);
		g_value_unset(&confval);

		if(tempconf == conf) {
		    
		    nbParticipant = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), &iter_parent);
		    DEBUG("nbParticipant: %i", nbParticipant);
		    for( j = 0; j < nbParticipant; j++) {
		        call = NULL;
			if(gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter_child, &iter_parent, j)){

			    callval.g_type = 0;
			    gtk_tree_model_get_value (GTK_TREE_MODEL(store), &iter_child, COLUMN_ACCOUNT_PTR, &callval);

			    call = (callable_obj_t*)g_value_get_pointer(&callval);
			    g_value_unset(&callval);
			    
			    if(call) {
			        calltree_add_call (tab, call, NULL);
			    }
			}
			
		    }

		    gtk_tree_store_remove(store, &iter_parent);
		}
	    }
	}
    }

    // callable_obj_t * selectedCall = calltab_get_selected_call(tab);
    // if(selectedCall == c)
    // calltab_select_call(tab, NULL);

    update_actions();
	
}


void calltree_display (calltab_t *tab) {

  
    GtkTreeSelection *sel;

    /* If we already are displaying the specified calltree */
    if (active_calltree == tab)
        return;

    /* case 1: we want to display the main calltree */
    if (tab==current_calls) {

        DEBUG ("display main tab");


	if (active_calltree==contacts) {
	    gtk_toggle_tool_button_set_active ((GtkToggleToolButton*)contactButton, FALSE);
	} else {
	    gtk_toggle_tool_button_set_active ((GtkToggleToolButton*)historyButton, FALSE);
	}
	// gtk_toggle_tool_button_set_active ((GtkToggleToolButton*)currentCallsButton, TRUE);

    }

    /* case 2: we want to display the history */
    else if (tab==history) {

        DEBUG ("display history tab");

	if (active_calltree==contacts) {
	    gtk_toggle_tool_button_set_active ((GtkToggleToolButton*)contactButton, FALSE);
	}

	gtk_toggle_tool_button_set_active ((GtkToggleToolButton*)historyButton, TRUE);
    }

    else if (tab==contacts) {

        DEBUG ("display contact tab");

	if (active_calltree==history) {
	    gtk_toggle_tool_button_set_active ((GtkToggleToolButton*)historyButton, FALSE);
	}

	gtk_toggle_tool_button_set_active ((GtkToggleToolButton*)contactButton, TRUE);
    }

    else
        ERROR ("calltree.c line %d . This is probably a bug in the application", __LINE__);
    

    gtk_widget_hide (active_calltree->tree);
    active_calltree = tab;
    gtk_widget_show (active_calltree->tree);
    
    sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (active_calltree->view));
    DEBUG("Emit signal changed from calltree_display");
    g_signal_emit_by_name(sel, "changed");
    update_actions();
}


void calltree_update_clock() {

  callable_obj_t *c = calltab_get_selected_call(current_calls);

  // if(!selected_call) {
  if(!c) {
    statusbar_update_clock("");
    return;
  }

  // if(!(selected_call->_timestr)) {
  if(!(c->_timestr)) {
    statusbar_update_clock("");
    return;
  }

  if( (c->_state != CALL_STATE_INVALID) &&
      (c->_state != CALL_STATE_INCOMING) &&
      (c->_state != CALL_STATE_RINGING) &&
      (c->_state != CALL_STATE_DIALING) &&
      (c->_state != CALL_STATE_FAILURE) &&
      (c->_state != CALL_STATE_BUSY) ) { 

    // TODO this make the whole thing crash...
    // calltree_update_call(current_calls, c, NULL);
    statusbar_update_clock(c->_timestr);
  }
  else {
    statusbar_update_clock("");
  }
}


static void drag_begin_cb(GtkWidget *widget, GdkDragContext *dc, gpointer data)
{

    GtkTargetList* target_list;

}

static void drag_end_cb(GtkWidget * widget, GdkDragContext * context, gpointer data)
{
    DEBUG("CallTree: Drag end callback");
    DEBUG("CallTree: selected_path %s, selected_call_id %s, selected_path_depth %d", 
	                             selected_path, selected_call_id, selected_path_depth);
    DEBUG("CallTree: dragged path %s, dragged_call_id %s, dragged_path_depth %d", 
                                     dragged_path, dragged_call_id, dragged_path_depth);

    GtkTreeModel *model = (GtkTreeModel*)current_calls->store;
    GtkTreePath *path = gtk_tree_path_new_from_string(dragged_path);
    GtkTreePath *dpath = gtk_tree_path_new_from_string(dragged_path);
    GtkTreePath *spath = gtk_tree_path_new_from_string(selected_path);
    
    GtkTreeIter iter;
    GtkTreeIter iter_parent;
    GtkTreeIter iter_children;
    GtkTreeIter parent_conference; // conference for which this call is attached

    GValue val;
    
    callable_obj_t* call;
    conference_obj_t* conf;


    // Make sure drag n drop does not imply a dialing call for either selected and dragged call
    if(selected_call && (selected_type == A_CALL)) {

      DEBUG("CallTree: Selected a call");
      if(selected_call->_state == CALL_STATE_DIALING || 
	 selected_call->_state == CALL_STATE_INVALID ||
	 selected_call->_state == CALL_STATE_FAILURE ||
	 selected_call->_state == CALL_STATE_BUSY ||
	 selected_call->_state == CALL_STATE_TRANSFERT) {
 
          DEBUG("CallTree: Selected an invalid call");

	  calltree_remove_call(current_calls, selected_call, NULL);
	  calltree_add_call(current_calls, selected_call, NULL);

	  dragged_call = NULL; 
	  return;
      }
    

      if(dragged_call && (dragged_type == A_CALL)) {

	DEBUG("CallTree: Dragged on a call");
	if(dragged_call->_state == CALL_STATE_DIALING || 
	   dragged_call->_state == CALL_STATE_INVALID ||
	   dragged_call->_state == CALL_STATE_FAILURE ||
	   dragged_call->_state == CALL_STATE_BUSY ||
	   dragged_call->_state == CALL_STATE_TRANSFERT) {
 
	    DEBUG("CallTree: Dragged on an invalid call");

	    calltree_remove_call(current_calls, selected_call, NULL);
	  
	    if(selected_call->_confID) {

	        gtk_tree_path_up(spath);
		gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &parent_conference, spath);
		
		calltree_add_call(current_calls, selected_call, &parent_conference);
	    }
	    else {

	        calltree_add_call(current_calls, selected_call, NULL);
	    }

	    dragged_call = NULL; 
	    return;
	}
      }

    }


    // Make sure a conference is only dragged on another conference
    if(selected_conf && (selected_type == A_CONFERENCE)) {

        DEBUG("CallTree: Selected a conference");

	if(!dragged_conf && (dragged_type == A_CALL)) {

	    DEBUG("CallTree: Dragged on a call");

	    conf = selected_conf;

	    calltree_remove_conference(current_calls, conf, NULL);
	    calltree_add_conference(current_calls, conf);

	    dragged_call = NULL;
	    return; 
	}
    }


    if(selected_path_depth == 1) {

        if(dragged_path_depth == 1) {

	  if (selected_type == A_CALL && dragged_type == A_CALL) {

	    if(gtk_tree_path_compare (dpath, spath) == 0) {
	        // draged a call on itself
	    }
	    else {
	      // dragged a single call on a single call
	      if(selected_call != NULL && dragged_call != NULL)
		sflphone_join_participant(selected_call->_callID, dragged_call->_callID);
	    }
	  }
	  else if(selected_type == A_CALL && dragged_type == A_CONFERENCE) {

	      // dragged a single call on a conference
	      if(!selected_call) {
	          DEBUG("Error: call dragged on a conference is null");
		  return;
	      }

	      selected_call->_confID = g_strdup(dragged_call_id);
	      sflphone_add_participant(selected_call_id, dragged_call_id);
	  }
	  else if(selected_type == A_CONFERENCE && dragged_type == A_CALL) {

	      // dragged a conference on a single call
	      conf = selected_conf;
			        
	      calltree_remove_conference(current_calls, conf, NULL);
	      calltree_add_conference(current_calls, conf);


	  }
	  else if(selected_type == A_CONFERENCE && dragged_type == A_CONFERENCE){

	      // dragged a conference on a conference
	      if(gtk_tree_path_compare (dpath, spath) == 0) {

		  if(!current_calls) {
		      DEBUG("Error while joining the same conference\n");
		      return;
		  }

		  DEBUG("Joined the same conference!\n");
		  gtk_tree_view_expand_row(GTK_TREE_VIEW(current_calls->view), path, FALSE);
	      }
	      else {

		  if(!selected_conf) {
		      DEBUG("Error: selected conference is null while joining 2 conference");
		  }

		  if(!dragged_conf) {
		      DEBUG("Error: dragged conference is null while joining 2 conference");
		  }

		  DEBUG("Joined two conference %s, %s!\n", dragged_path, selected_path);
		  sflphone_join_conference(selected_conf->_confID, dragged_conf->_confID);
	      }
	  }

	  // TODO: dragged a single call on a NULL element (should do nothing)
	  // TODO: dragged a conference on a NULL element (should do nothing)

	}
	else {
 
	    // dragged_path_depth == 2
	  if (selected_type == A_CALL && dragged_type == A_CALL) {

	      // TODO: dragged a call on a conference call
	      calltree_remove_call(current_calls, selected_call, NULL);
	      calltree_add_call(current_calls, selected_call, NULL);
	  }

	  else if(selected_type == A_CONFERENCE && dragged_type == A_CALL) {

	      // TODO: dragged a conference on a conference call
	      calltree_remove_conference(current_calls, selected_conf, NULL);
	      calltree_add_conference(current_calls, selected_conf);
	  }

	  // TODO: dragged a single call on a NULL element 
	  // TODO: dragged a conference on a NULL element
	}
    }
    else {

        // selected_path_depth == 2
      
        if(dragged_path_depth == 1) {

	    if(selected_type == A_CALL && dragged_type == A_CALL) {

	      // dragged a conference call on a call
	      sflphone_detach_participant(selected_call_id);
		    
	      if(selected_call != NULL && dragged_call != NULL)
		  sflphone_join_participant(selected_call->_callID, dragged_call->_callID);

	    }
	    else if(selected_type == A_CALL && dragged_type == A_CONFERENCE) {

	        // dragged a conference call on a conference
	        sflphone_detach_participant(selected_call_id);
	      
		if(selected_call != NULL && dragged_conf != NULL) {

		    DEBUG("Adding a participant, since dragged call on a conference");

		    sflphone_add_participant(selected_call_id, dragged_call_id);
		}
	    }
	    else {

	        // dragged a conference call on a NULL element
	        sflphone_detach_participant(selected_call_id);
	    }
		
	}
	else {

	    // dragged_path_depth == 2
	    // dragged a conference call on another conference call (same conference)
	    // TODO: dragged a conference call on another conference call (different conference)
	      
	    gtk_tree_path_up(path);

	    gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &parent_conference, path);

	    gtk_tree_path_up(dpath);
	    gtk_tree_path_up(spath);
		
	    if(gtk_tree_path_compare (dpath, spath) == 0) {

	        DEBUG("Dragged a call in the same conference");
		calltree_remove_call (current_calls, selected_call, NULL);
		calltree_add_call (current_calls, selected_call, &parent_conference);
	    }
	    else {


	        DEBUG("Dragged a conference call onto another conference call %s, %s", gtk_tree_path_to_string(dpath), gtk_tree_path_to_string(spath));

		conf = NULL;
		
		val.g_type = 0;
		if(gtk_tree_model_get_iter (model, &iter, dpath)) {
		  
		    DEBUG("we got an iter!");
		    gtk_tree_model_get_value (model, &iter, COLUMN_ACCOUNT_PTR, &val);
		  
		    conf = (conference_obj_t*)g_value_get_pointer(&val);
		}
		g_value_unset(&val);

		sflphone_detach_participant(selected_call_id);

		if(conf) {

		    DEBUG("we got a conf!");
		    sflphone_add_participant(selected_call_id, conf->_confID);
		}
		else {

		    DEBUG("didn't find a conf!");
		}
	    }

	    // TODO: dragged a conference call on another conference call (different conference)
	    // TODO: dragged a conference call on a NULL element (same conference)
	    // TODO: dragged a conference call on a NULL element (different conference)
	}
	
    }	

}


void drag_data_received_cb(GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *selection_data, guint info, guint t, gpointer data)
{

    // DEBUG("drag_data_received_cb\n");
    GtkTreeView *tree_view = GTK_TREE_VIEW(widget);
    GtkTreePath *drop_path;
    GtkTreeViewDropPosition position;
    GValue val;

    GtkTreeModel *model = (GtkTreeModel*)active_calltree->store;
    GtkTreeModel* tree_model = gtk_tree_view_get_model(tree_view);

    GtkTreeIter iter;
    gchar value;


    val.g_type = 0;
    gtk_tree_view_get_drag_dest_row(tree_view, &drop_path, &position);

    if(drop_path) {

        gtk_tree_model_get_iter(tree_model, &iter, drop_path);
	gtk_tree_model_get_value(tree_model, &iter, COLUMN_ACCOUNT_PTR, &val);

	    
	if(gtk_tree_model_iter_has_child(tree_model, &iter)) {

	    DEBUG("CallTree: Dragging on a conference");
	    dragged_type = A_CONFERENCE;
	    dragged_call = NULL;
	}
	else {

	    DEBUG("CallTree: Dragging on a call");
	    dragged_type = A_CALL;
	    dragged_conf = NULL;
	}

	switch (position)  {

	case GTK_TREE_VIEW_DROP_AFTER:
	    DEBUG("CallTree: GTK_TREE_VIEW_DROP_AFTER");
	    dragged_path = gtk_tree_path_to_string(drop_path);
	    dragged_path_depth = gtk_tree_path_get_depth(drop_path);
	    dragged_call_id = "NULL";
	    dragged_call = NULL;
	    dragged_conf = NULL;
	    break;

	case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
	    DEBUG("CallTree: GTK_TREE_VIEW_DROP_INTO_OR_AFTER");
	    dragged_path = gtk_tree_path_to_string(drop_path);
	    dragged_path_depth = gtk_tree_path_get_depth(drop_path);
	    if (dragged_type == A_CALL) {
	      
	        dragged_call_id = ((callable_obj_t*)g_value_get_pointer(&val))->_callID;
		dragged_call = (callable_obj_t*)g_value_get_pointer(&val);
	    }
	    else {

	        dragged_call_id = ((conference_obj_t*)g_value_get_pointer(&val))->_confID;
		dragged_conf = (conference_obj_t*)g_value_get_pointer(&val);
	    }
	    break;

	case GTK_TREE_VIEW_DROP_BEFORE:
	    DEBUG("CallTree: GTK_TREE_VIEW_DROP_BEFORE");
	    dragged_path = gtk_tree_path_to_string(drop_path);
	    dragged_path_depth = gtk_tree_path_get_depth(drop_path);
	    dragged_call_id = "NULL";
	    dragged_call = NULL;
	    dragged_conf = NULL;
	    break;

	case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
	    DEBUG("CallTree: GTK_TREE_VIEW_DROP_INTO_OR_BEFORE");
	    dragged_path = gtk_tree_path_to_string(drop_path);
	    dragged_path_depth = gtk_tree_path_get_depth(drop_path);
	    if (dragged_type == A_CALL) {
	        dragged_call_id = ((callable_obj_t*)g_value_get_pointer(&val))->_callID;
		dragged_call = (callable_obj_t*)g_value_get_pointer(&val);
	    }
	    else {
	      dragged_call_id = ((conference_obj_t*)g_value_get_pointer(&val))->_confID;
	      dragged_conf = (conference_obj_t*)g_value_get_pointer(&val);
	    }
	    break;
	    
	default:
	  return;
	}
    }
}
