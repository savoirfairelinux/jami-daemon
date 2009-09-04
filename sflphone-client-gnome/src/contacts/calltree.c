/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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

#include <calltree.h>
#include <stdlib.h>
#include <glib/gprintf.h>
#include <calllist.h>
#include <conferencelist.h>
#include <toolbar.h>
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
selected(GtkTreeSelection *sel, void* data UNUSED )
{

    DEBUG("Selection Callback");

    GtkTreeIter iter;
    GValue val;
    GtkTreeModel *model = (GtkTreeModel*)active_calltree->store;

    GtkTreePath* path;
    gchar* string_path;


    if (! gtk_tree_selection_get_selected (sel, &model, &iter))
        return;

    // store info for dragndrop
    path = gtk_tree_model_get_path(model, &iter);
    string_path = gtk_tree_path_to_string(path);
    selected_path_depth = gtk_tree_path_get_depth(path);

    if(gtk_tree_model_iter_has_child(GTK_TREE_MODEL(model), &iter))
    {
	DEBUG("SELECTED A CONFERENCE");
	selected_type = A_CONFERENCE;

	val.g_type = 0;
	gtk_tree_model_get_value (model, &iter, 2, &val);

	calltab_select_conf((conference_obj_t*) g_value_get_pointer(&val));

	selected_conf = (conference_obj_t*)g_value_get_pointer(&val);

	selected_call_id = selected_conf->_confID;
	selected_path = string_path;
    }
    else
    {
	DEBUG("SELECTED A CALL");
	selected_type = A_CALL;
	// gtk_tree_model_iter_parent(GTK_TREE_MODEL(model), parent_conference, &iter);

	val.g_type = 0;
        gtk_tree_model_get_value (model, &iter, 2, &val);

        calltab_select_call(active_calltree, (callable_obj_t*) g_value_get_pointer(&val));

        selected_call = (callable_obj_t*)g_value_get_pointer(&val);

	selected_call_id = selected_call->_callID;
	selected_path = string_path;
    }

    DEBUG("selected_cb\n");
    DEBUG("  selected_path %s, selected_call_id %s, selected_path_depth %i\n", selected_path, selected_call_id, selected_path_depth);

    // conferencelist_reset ();
    // sflphone_fill_conference_list();

    g_value_unset(&val);
    toolbar_update_buttons();

}

/* A row is activated when it is double clicked */
void  row_activated(GtkTreeView       *tree_view UNUSED,
        GtkTreePath       *path UNUSED,
        GtkTreeViewColumn *column UNUSED,
        void * data UNUSED)
{
    callable_obj_t* selectedCall = NULL;
    callable_obj_t* new_call;
    conference_obj_t* selectedConf = NULL;
    gchar *account_id;

    DEBUG("double click action");

    if( active_calltree == current_calls )
    {
	
	if(calltab_get_selected_type(current_calls) == A_CALL)
	{
	    selectedCall = calltab_get_selected_call(current_calls);

	    if (selectedCall)
	    {
		// Get the right event from the right calltree
		if( active_calltree == current_calls )
		{
		    switch(selectedCall->_state)
		    {
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
		else
		{
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
	else
	{
	    selectedConf = calltab_get_selected_conf(current_calls);
 
	    if(selectedConf)
	    {
		switch(selectedConf->_state)
		{
	            case CONFERENCE_STATE_ACTIVE_ATACHED:
		        sflphone_add_main_participant(selectedConf);
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

    static gboolean
button_pressed(GtkWidget* widget, GdkEventButton *event, gpointer user_data UNUSED)
{
    if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
    {
        if( active_calltree == current_calls )
        {
            show_popup_menu(widget,  event);
            return TRUE;
        }
        else if (active_calltree == history)
        {
            show_popup_menu_history (widget,  event);
            return TRUE;
        }
        else{
            show_popup_menu_contacts (widget, event);
            return TRUE;
        }
    }
    return FALSE;
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
    DEBUG("set_focus_on_calltree_out");
    // gtk_widget_grab_focus(GTK_WIDGET(sw));
    focus_is_on_calltree = FALSE;
}

void
focus_on_calltree_in(){
    DEBUG("set_focus_on_calltree_in");
    // gtk_widget_grab_focus(GTK_WIDGET(sw));
    focus_is_on_calltree = TRUE;
}

    void
calltree_create (calltab_t* tab, gboolean searchbar_type)
{
    // GtkWidget *sw;
    // GtkCellRenderer *rend;
    // GtkTreeViewColumn *col;
    // GtkTreeSelection *sel;

    tab->tree = gtk_vbox_new(FALSE, 10);

    // Fix bug #708 (resize)
    gtk_widget_set_usize(tab->tree,100,80);

    gtk_container_set_border_width (GTK_CONTAINER (tab->tree), 0);

    sw = gtk_scrolled_window_new( NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);

    tab->store = gtk_tree_store_new (3,
            GDK_TYPE_PIXBUF,// Icon
            G_TYPE_STRING,  // Description
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


    if(tab != history || tab!=contacts) {

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
            "markup", 1,
            NULL);
    gtk_tree_view_append_column (GTK_TREE_VIEW(tab->view), col);

    g_object_unref(G_OBJECT(tab->store));
    gtk_container_add(GTK_CONTAINER(sw), tab->view);

    sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (tab->view));
    g_signal_connect (G_OBJECT (sel), "changed",
            G_CALLBACK (selected),
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

    DEBUG("calltree_remove_call %s", c->_callID);

    GtkTreeIter iter;
    GValue val;
    callable_obj_t * iterCall;
    GtkTreeStore* store = tab->store;

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
            gtk_tree_model_get_value (GTK_TREE_MODEL(store), &iter, 2, &val);

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
    toolbar_update_buttons();
}

    void
    calltree_update_call (calltab_t* tab, callable_obj_t * c, GtkTreeIter *parent)
{
    GdkPixbuf *pixbuf=NULL;
    GtkTreeIter iter;
    GValue val;
    callable_obj_t * iterCall;
    GtkTreeStore* store = tab->store;

    
    int nbChild = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), parent);
    int i;

    for( i = 0; i < nbChild; i++)
    {

        if(gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter, parent, i))
        {

	    if(gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), &iter))
	    {
		calltree_update_call (tab, c, &iter);
	    }

            val.g_type = 0;
            gtk_tree_model_get_value (GTK_TREE_MODEL(store), &iter, 2, &val);

            iterCall = (callable_obj_t*) g_value_get_pointer(&val);
            g_value_unset(&val);

            if(iterCall == c)
            {
                // Existing call in the list
                gchar * description;
                gchar * date="";
                gchar * duration="";
                if(c->_state == CALL_STATE_TRANSFERT)
                {
                    description = g_markup_printf_escaped("<b>%s</b> <i>%s</i>\n<i>%s%s</i> ",
                            c->_peer_number,
                            c->_peer_name,
                            _("Transfer to : "),
                            c->_trsft_to
                            );
                }
                else
                {
                    description = g_markup_printf_escaped("<b>%s</b> <i>%s</i>",
                            c->_peer_number,
                            c->_peer_name );

                }

                if( tab == current_calls )
                {
                    switch(c->_state)
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
                        case CALL_STATE_RECORD:
                            pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/rec_call.svg", NULL);
                            break;
                        default:
                            WARN("Update calltree - Should not happen!");
                    }
                }
                else
                {
                    switch(c->_history_state)
                    {
                        case OUTGOING:
                            DEBUG("Outgoing state");
                            pixbuf = gdk_pixbuf_new_from_file( ICONS_DIR "/outgoing.svg", NULL);
                            break;
                        case INCOMING:
                            DEBUG("Incoming state");
                            pixbuf = gdk_pixbuf_new_from_file( ICONS_DIR "/incoming.svg", NULL);
                            break;
                        case MISSED:
                            DEBUG("Missed state");
                            pixbuf = gdk_pixbuf_new_from_file( ICONS_DIR "/missed.svg", NULL);
                            break;
                        default:
                            DEBUG("No history state");
                            break;
                    }
                    date = get_formatted_start_timestamp (c);
                    duration = get_call_duration (c);
                    duration = g_strconcat( date , duration , NULL);
                    description = g_strconcat( description , duration, NULL);
                }
                //Resize it
                if(pixbuf)
                {
                    if(gdk_pixbuf_get_width(pixbuf) > 32 || gdk_pixbuf_get_height(pixbuf) > 32)
                    {
                        pixbuf =  gdk_pixbuf_scale_simple(pixbuf, 32, 32, GDK_INTERP_BILINEAR);
                    }
                }
                gtk_tree_store_set(store, &iter,
                        0, pixbuf, // Icon
                        1, description, // Description
                        -1);

                if (pixbuf != NULL)
                    g_object_unref(G_OBJECT(pixbuf));

            }
        }

    }
    toolbar_update_buttons();
}

void calltree_add_history_entry (callable_obj_t * c)
{

    if (dbus_get_history_enabled () == 0)
        return;

    GdkPixbuf *pixbuf=NULL;
    GtkTreeIter iter;

    // New call in the list
    gchar * description, *date="", *duration="";
    description = g_markup_printf_escaped("<b>%s</b> <i>%s</i>",
            c->_peer_number,
            c->_peer_name);

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
    gtk_tree_store_set(history->store, &iter,
            0, pixbuf, // Icon
            1, description, // Description
            2, c,      // Pointer
            -1);

    if (pixbuf != NULL)
        g_object_unref(G_OBJECT(pixbuf));

    gtk_tree_view_set_model (GTK_TREE_VIEW(history->view), GTK_TREE_MODEL(history->store));
    
    history_reinit(history);
}


void calltree_add_call (calltab_t* tab, callable_obj_t * c, GtkTreeIter *parent)
{

    DEBUG("calltree_add_call %s", c->_callID);

    if (tab == history)
    {
        calltree_add_history_entry (c);
        return;
    }

    GdkPixbuf *pixbuf=NULL;
    GtkTreeIter iter;

    // New call in the list
    gchar * description;
    gchar * date="";
    gchar *duration="";
    description = g_markup_printf_escaped("<b>%s</b> <i>%s</i>",
            c->_peer_number,
            c->_peer_name);

    gtk_tree_store_prepend (tab->store, &iter, parent);


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
    gtk_tree_store_set(tab->store, &iter,
            0, pixbuf, // Icon
            1, description, // Description
            2, c,      // Pointer
            -1);


    if (pixbuf != NULL)
        g_object_unref(G_OBJECT(pixbuf));


    // history_reinit (tab);

    // sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tab->view));
    // gtk_tree_selection_select_iter(GTK_TREE_SELECTION(sel), &iter);

    // history_reinit (tab);

    gtk_tree_view_set_model(GTK_TREE_VIEW(tab->view), GTK_TREE_MODEL(tab->store));

    // gtk_tree_view_set_model (GTK_TREE_VIEW (tab->view), GTK_TREE_MODEL (history_filter));

    gtk_tree_selection_select_iter(gtk_tree_view_get_selection(GTK_TREE_VIEW(tab->view)), &iter);

    toolbar_update_buttons();

}


void calltree_add_conference (calltab_t* tab, const conference_obj_t* conf)
{

    DEBUG("calltree_add_conference conf->_confID %s\n", conf->_confID);


    GdkPixbuf *pixbuf=NULL;
    GtkTreeIter iter;
    GtkTreePath *path;
    GtkTreeModel *model = (GtkTreeModel*)active_calltree->store;

    gchar** participant = (gchar**)dbus_get_participant_list(conf->_confID);
    gchar** pl;
    gchar* call_id;

    callable_obj_t * call;

    // New call in the list
    
    gchar * description;
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
    
    else {
        WARN ("Conferences cannot be added in this widget - This is a bug in the application.");
    }

    DEBUG("PIXWITH: %i\n", gdk_pixbuf_get_width(pixbuf));
    //Resize it
    if(pixbuf)
    {
        if(gdk_pixbuf_get_width(pixbuf) > 32 || gdk_pixbuf_get_height(pixbuf) > 32)
        {
            pixbuf =  gdk_pixbuf_scale_simple(pixbuf, 32, 32, GDK_INTERP_BILINEAR);
        }
    }
    
    gtk_tree_store_set(tab->store, &iter,
            0, pixbuf, // Icon
            1, description, // Description
	    2, conf, // Pointer
            -1);

    
    if (pixbuf != NULL)
        g_object_unref(G_OBJECT(pixbuf));


    if(participant)
    {
	for (pl = participant; *participant; participant++)
	{
	    
	    call_id = (gchar*)(*participant);
	    call = calllist_get (tab, call_id);
	    // create_new_call_from_details (conf_id, conference_details, &c);

	    calltree_remove_call(tab, call, NULL);
	    calltree_add_call (tab, call, &iter);
	}
    }

    gtk_tree_view_set_model(GTK_TREE_VIEW(tab->view), GTK_TREE_MODEL(tab->store));

    path = gtk_tree_model_get_path(model, &iter);

    gtk_tree_view_expand_row(GTK_TREE_VIEW(tab->view), path, FALSE);

    toolbar_update_buttons();

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
    for( i = 0; i < nbChild; i++)
    {
	
        if(gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter_parent, parent, i))
        {
	    
	    if (gtk_tree_model_iter_has_child(GTK_TREE_MODEL(store), &iter_parent))
	    {
		
		calltree_remove_conference (tab, conf, &iter_parent);
	    
		confval.g_type = 0;
		gtk_tree_model_get_value (GTK_TREE_MODEL(store), &iter_parent, 2, &confval);

		tempconf = (conference_obj_t*) g_value_get_pointer(&confval);
		g_value_unset(&confval);

		if(tempconf == conf)
		{
		    nbParticipant = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), &iter_parent);
		    DEBUG("nbParticipant: %i\n", nbParticipant);
		    for( j = 0; j < nbParticipant; j++)
		    {
			call = NULL;
			if(gtk_tree_model_iter_nth_child(GTK_TREE_MODEL(store), &iter_child, &iter_parent, j))
			{
			    
			    callval.g_type = 0;
			    gtk_tree_model_get_value (GTK_TREE_MODEL(store), &iter_child, 2, &callval);
			    
			    call = (callable_obj_t*)g_value_get_pointer(&callval);
			    g_value_unset(&callval);
			    
			    if(call)
			    {
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

    toolbar_update_buttons();
    
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
    g_signal_emit_by_name(sel, "changed");
    toolbar_update_buttons();
}




static void drag_begin_cb(GtkWidget *widget, GdkDragContext *dc, gpointer data)
{

    GtkTargetList* target_list;

    // g_print("drag_begin_cb %s\n", dragged_path);
    if((target_list = gtk_drag_source_get_target_list(widget)) != NULL);
        
    
}

static void drag_end_cb(GtkWidget * widget, GdkDragContext * context, gpointer data)
{
    DEBUG("drag_end_cb\n");
    DEBUG("    selected_path %s, selected_call_id %s, selected_path_depth %i\n", selected_path, selected_call_id, selected_path_depth);
    DEBUG("    dragged path %s, dragged_call_id %s, dragged_path_depth %i\n", selected_path, selected_call_id, dragged_path_depth);

    GtkTreeModel* model = (GtkTreeModel*)current_calls->store;
    GtkTreePath *path = gtk_tree_path_new_from_string(dragged_path);

    GtkTreeIter iter_parent;
    GtkTreeIter iter_children;
    GtkTreeIter parent_conference;

    GValue val;


    if(selected_path_depth == 1)
    {
        if(dragged_path_depth == 1)
        {
	    
	    if (selected_type == A_CALL && dragged_type == A_CALL) 
	    {
		// dragged a single call on a single call
	        if(selected_call != NULL && dragged_call != NULL)
		    sflphone_join_participant(selected_call->_callID, dragged_call->_callID);
	    }
	    else if(selected_type == A_CALL && dragged_type == A_CONFERENCE)
	    {
		// TODO: dragged a single call on a conference
		sflphone_add_participant(selected_call_id, dragged_call_id);
	    }
	    else if(selected_type == A_CONFERENCE && dragged_type == A_CALL)
	    {
		// TODO: dragged a conference on a single call (make no sence)
		// calltree_add_conference(current_calls, selected_conf);
		calltree_remove_conference(current_calls, selected_conf, NULL);
		calltree_add_conference(current_calls, selected_conf);
		
		
	    }
	    else if(selected_type == A_CONFERENCE && dragged_type == A_CONFERENCE)
	    {
		// TODO: dragged a conference on a conference
	    }

	    // TODO: dragged a single call on a NULL element (should do nothing)
	    // TODO: dragged a conference on a NULL element (should do nothing)
	
        }
	else // dragged_path_depth == 2
	{
	    if (selected_type == A_CALL && dragged_type == A_CALL)
	    {
		// TODO: dragged a call on a conference call
		calltree_remove_call(current_calls, selected_call, NULL);
		calltree_add_call(current_calls, selected_call, NULL);
	    }
	    else if(selected_type == A_CONFERENCE && dragged_type == A_CALL)
	    {
		// TODO: dragged a conference on a conference call
		calltree_remove_conference(current_calls, selected_conf, NULL);
		calltree_add_conference(current_calls, selected_conf);
	    }
 
	    // TODO: dragged a single call on a NULL element 
	    // TODO: dragged a conference on a NULL element
	}
    }
    else // selected_path_depth == 2
    {
	
	if(dragged_path_depth == 1)
	{
	    
	    if(selected_type == A_CALL && dragged_type == A_CALL)
	    {
		// TODO: dragged a conference call on a call
    
	    }
	    else if(selected_type == A_CALL && dragged_type == A_CONFERENCE)
	    {
	        // TODO: dragged a conference call on a conference
	    }
	    else
	    {
		// TODO: dragged a conference call on a NULL element
		// sflphone_detach_participant(selected_call_id);
	    }
	    sflphone_detach_participant(selected_call_id);
	}
	else // dragged_path_depth == 2
	{
	    // dragged a conference call on another conference call (same conference)
	    // TODO: dragged a conference call on another conference call (different conference)
	    DEBUG("NON-AUTHORIZED DRAG");
	    gtk_tree_path_up(path);

	    gtk_tree_model_get_iter(GTK_TREE_MODEL(model), &parent_conference, path);

	    calltree_remove_call (current_calls, selected_call, NULL);
	    calltree_add_call (current_calls, selected_call, &parent_conference);

	    // TODO: dragged a conference call on another conference call (different conference)
	    // TODO: dragged a conference call on a NULL element (same conference)
	    // TODO: dragged a conference call on a NULL element (different conference)
	}

    }	
    
}


void drag_data_received_cb(GtkWidget *widget, GdkDragContext *context, gint x, gint y, GtkSelectionData *selection_data, guint info, guint t, gpointer data)
{

    // g_print("drag_data_received_cb\n");
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

    if(drop_path)
    {

        gtk_tree_model_get_iter(tree_model, &iter, drop_path);
        gtk_tree_model_get_value(tree_model, &iter, 2, &val);


	if(gtk_tree_model_iter_has_child(tree_model, &iter))
	{
	    DEBUG("DRAGGING ON A CONFERENCE");
	    dragged_type = A_CONFERENCE;
	}
	else
	{
	    DEBUG("DRAGGING ON A CALL");
	    dragged_type = A_CALL;
	}

        switch (position) 
        {
            case GTK_TREE_VIEW_DROP_AFTER:
                dragged_path = gtk_tree_path_to_string(drop_path);
		dragged_path_depth = gtk_tree_path_get_depth(drop_path);
		dragged_call_id = "NULL";
		dragged_call = NULL;
                g_print("    AFTER dragged_path %s, dragged_call_id %s, dragged_path_depth %i\n", dragged_path, dragged_call_id, dragged_path_depth);
                break;

            case GTK_TREE_VIEW_DROP_INTO_OR_AFTER:
                dragged_path = gtk_tree_path_to_string(drop_path);
		dragged_path_depth = gtk_tree_path_get_depth(drop_path);
		if (dragged_type == A_CALL)
		{
		    dragged_call_id = ((callable_obj_t*)g_value_get_pointer(&val))->_callID;
		    dragged_call = (callable_obj_t*)g_value_get_pointer(&val);
		}
		else
		{
		    dragged_call_id = ((conference_obj_t*)g_value_get_pointer(&val))->_confID;
		}
                g_print("    INTO_OR_AFTER dragged_path %s, dragged_call_id %s, dragged_path_depth %i\n", dragged_path, dragged_call_id, dragged_path_depth);
                break;

            case GTK_TREE_VIEW_DROP_BEFORE:
                dragged_path = gtk_tree_path_to_string(drop_path);
		dragged_path_depth = gtk_tree_path_get_depth(drop_path);
		dragged_call_id = "NULL";
		dragged_call = NULL;
                g_print("    BEFORE dragged_path %s, dragged_call_id %s, dragged_path_depth %i\n", dragged_path, dragged_call_id, dragged_path_depth);
                break;

            case GTK_TREE_VIEW_DROP_INTO_OR_BEFORE:
                dragged_path = gtk_tree_path_to_string(drop_path);
		dragged_path_depth = gtk_tree_path_get_depth(drop_path);
		if (dragged_type == A_CALL)
		{
		    dragged_call_id = ((callable_obj_t*)g_value_get_pointer(&val))->_callID;
		    dragged_call = (callable_obj_t*)g_value_get_pointer(&val);
		}
		else
		{
		    dragged_call_id = ((conference_obj_t*)g_value_get_pointer(&val))->_confID;
		}
                g_print("    INTO_OR_BEFORE dragged_path %s, dragged_call_id %s, dragged_path_depth %i\n", dragged_path, dragged_call_id, dragged_path_depth);
                break;

            default:
                return;
        }
    }
}
