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

#include <stdlib.h>
#include <gtk/gtk.h>
#include <actions.h>
#include <calltree.h>
#include <calllist.h>
#include <menus.h>
#include <dbus.h>



GtkWidget   * toolbar;
GtkToolItem * pickupButton;
GtkToolItem * callButton;
GtkToolItem * hangupButton;
GtkToolItem * holdButton;
GtkToolItem * transfertButton;
GtkToolItem * unholdButton;
GtkToolItem * historyButton;
GtkToolItem * mailboxButton;
guint transfertButtonConnId; //The button toggled signal connection ID
gboolean history_shown;

  void
switch_tab()
{
  (gtk_toggle_tool_button_get_active(GTK_TOGGLE_TOOL_BUTTON(historyButton)))? 
    gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(historyButton), FALSE):
    gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(historyButton), TRUE);
}

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
is_inserted( GtkWidget* button )
{
  return ( GTK_WIDGET(button)->parent == GTK_WIDGET( toolbar ) );
}

  static gboolean
button_pressed(GtkWidget* widget, GdkEventButton *event, gpointer user_data)
{
  if (event->button == 3 && event->type == GDK_BUTTON_PRESS)
  {
    if( active_calltree == current_calls )
    {
      show_popup_menu(widget,  event);
      return TRUE;
    }
    else
    {
      show_popup_menu_history(widget,  event);
      return TRUE;
    }
  }
  return FALSE;
}
/**
 * Make a call
 */
  static void 
call_button( GtkWidget *widget, gpointer   data )
{
  call_t * selectedCall = call_get_selected(active_calltree);
  call_t* newCall =  g_new0 (call_t, 1);
  printf("Call button pressed\n");
  if(call_list_get_size(current_calls)>0)
    sflphone_pick_up();
  else if(call_list_get_size(active_calltree) > 0){
    if( selectedCall)
    {
      printf("Calling a called num\n");

      newCall->to = g_strdup(call_get_number(selectedCall));
      newCall->from = g_strconcat("\"\" <", call_get_number(selectedCall), ">",NULL);
      newCall->state = CALL_STATE_DIALING;
      newCall->callID = g_new0(gchar, 30);
      g_sprintf(newCall->callID, "%d", rand()); 
      newCall->_start = 0;
      newCall->_stop = 0;

      printf("call : from : %s to %s\n", newCall->from, newCall->to);
      call_list_add(current_calls, newCall);
      update_call_tree_add(current_calls, newCall);
      sflphone_place_call(newCall);
      if( active_calltree == history )  switch_tab();
    }
    else
    {
      sflphone_new_call();
      if( active_calltree == history )  switch_tab();
    }
  }
  else
  {
    sflphone_new_call();
    if( active_calltree == history )  switch_tab();
  }
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

  static void
toggle_history(GtkToggleToolButton *toggle_tool_button,
    gpointer	user_data)
{
	GtkTreeSelection *sel;
	if(history_shown){
		active_calltree = current_calls;
		gtk_widget_hide(history->tree);
		gtk_widget_show(current_calls->tree);
		history_shown = FALSE;
	}else{
		active_calltree = history;
		gtk_widget_hide(current_calls->tree);
		gtk_widget_show(history->tree);
		history_shown = TRUE;
	}
	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (active_calltree->view));
	g_signal_emit_by_name(sel, "changed");
	toolbar_update_buttons();
	gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(histfilter));

}

  static void
call_mailbox( GtkWidget* widget , gpointer data )
{
  account_t* current = account_list_get_current();
  if( current == NULL ) // Should not happens
    return; 
  call_t* mailboxCall = g_new0( call_t , 1);
  mailboxCall->state = CALL_STATE_DIALING;
  mailboxCall->to = g_strdup(g_hash_table_lookup(current->properties, ACCOUNT_MAILBOX));
  mailboxCall->from = g_markup_printf_escaped(_("\"Voicemail\" <%s>"),  mailboxCall->to);
  mailboxCall->callID = g_new0(gchar, 30);
  g_sprintf(mailboxCall->callID, "%d", rand());
  mailboxCall->accountID = g_strdup(current->accountID);
  mailboxCall->_start = 0;
  mailboxCall->_stop = 0;
  g_print("TO : %s\n" , mailboxCall->to);
  call_list_add( current_calls , mailboxCall );
  update_call_tree_add( current_calls , mailboxCall );    
  update_menus();
  sflphone_place_call( mailboxCall );
  if( active_calltree == history )  switch_tab();
}

  void 
toolbar_update_buttons ()
{
  gtk_widget_set_sensitive( GTK_WIDGET(callButton),       FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(holdButton),       FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(transfertButton),  FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(mailboxButton) ,   FALSE);
  gtk_widget_set_sensitive( GTK_WIDGET(unholdButton),     FALSE);
  g_object_ref(holdButton);
  g_object_ref(unholdButton);
  if( is_inserted( GTK_WIDGET(holdButton) ) )   gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(holdButton));
  if( is_inserted( GTK_WIDGET(unholdButton) ) )	gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(unholdButton));
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), holdButton, 3);
  g_object_ref(callButton);
  g_object_ref(pickupButton);
  if( is_inserted( GTK_WIDGET(callButton) ) )	gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(callButton));
  if( is_inserted( GTK_WIDGET(pickupButton) ) )	gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(pickupButton));
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), callButton, 0);


  gtk_signal_handler_block(GTK_OBJECT(transfertButton),transfertButtonConnId);
  gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(transfertButton), FALSE);
  gtk_signal_handler_unblock(transfertButton, transfertButtonConnId);

  call_t * selectedCall = call_get_selected(active_calltree);
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
	if( active_calltree != history )  gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     TRUE);
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
      gtk_widget_set_sensitive( GTK_WIDGET(callButton), TRUE );
      gtk_widget_set_sensitive( GTK_WIDGET(mailboxButton), TRUE );
    }
    else
    {
      gtk_widget_set_sensitive( GTK_WIDGET(callButton), FALSE);
    }
  }
}
/* Call back when the user click on a call in the list */
  static void 
selected(GtkTreeSelection *sel, void* data) 
{
  GtkTreeIter  iter;
  GValue val;
  GtkTreeModel *model = (GtkTreeModel*)active_calltree->store;

  if (! gtk_tree_selection_get_selected (sel, &model, &iter))
    return;

  val.g_type = 0;
  gtk_tree_model_get_value (model, &iter, 2, &val);

  call_select(active_calltree, (call_t*) g_value_get_pointer(&val));
  g_value_unset(&val);

  toolbar_update_buttons();
}

/* A row is activated when it is double clicked */
void  row_activated(GtkTreeView       *tree_view,
    GtkTreePath       *path,
    GtkTreeViewColumn *column,
    void * data) 
{
  g_print("double click action\n");
  call_t* selectedCall;
  call_t* newCall;
  selectedCall = call_get_selected( active_calltree );

  if (selectedCall)
  {
    // Get the right event from the right calltree
    if( active_calltree == current_calls )
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
    else
    {
      newCall = g_new0( call_t, 1 );
      newCall->to = g_strdup(call_get_number(selectedCall));
      newCall->from = g_strconcat("\"\" <", call_get_number(selectedCall), ">",NULL);
      newCall->state = CALL_STATE_DIALING;
      newCall->callID = g_new0(gchar, 30);
      g_sprintf(newCall->callID, "%d", rand()); 
      newCall->_start = 0;
      newCall->_stop = 0;

      printf("call : from : %s to %s\n", newCall->from, newCall->to);
      call_list_add(current_calls, newCall);
      update_call_tree_add(current_calls, newCall);
      sflphone_place_call(newCall);
      switch_tab();
    }
  }
}

  GtkWidget * 
create_toolbar ()
{
  GtkWidget *ret;
  GtkWidget *image;

  ret = gtk_toolbar_new();
  toolbar = ret;

  gtk_toolbar_set_orientation(GTK_TOOLBAR(ret), GTK_ORIENTATION_HORIZONTAL);
  gtk_toolbar_set_style(GTK_TOOLBAR(ret), GTK_TOOLBAR_ICONS);

  image = gtk_image_new_from_file( ICONS_DIR "/call.svg");
  callButton = gtk_tool_button_new (image, _("Place a call"));
#if GTK_CHECK_VERSION(2,12,0)
  gtk_widget_set_tooltip_text(GTK_WIDGET(callButton), _("Place a call"));
#endif
  g_signal_connect (G_OBJECT (callButton), "clicked",
      G_CALLBACK (call_button), NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(ret), GTK_TOOL_ITEM(callButton), -1);

  image = gtk_image_new_from_file( ICONS_DIR "/accept.svg");
  pickupButton = gtk_tool_button_new(image, _("Pick up"));
#if GTK_CHECK_VERSION(2,12,0)
  gtk_widget_set_tooltip_text(GTK_WIDGET(pickupButton), _("Pick up"));
#endif
  gtk_widget_set_state( GTK_WIDGET(pickupButton), GTK_STATE_INSENSITIVE);
  g_signal_connect(G_OBJECT (pickupButton), "clicked", 
      G_CALLBACK (call_button), NULL);
  gtk_widget_show_all(GTK_WIDGET(pickupButton));

  image = gtk_image_new_from_file( ICONS_DIR "/hang_up.svg");
  hangupButton = gtk_tool_button_new (image, _("Hang up"));
#if GTK_CHECK_VERSION(2,12,0)
  gtk_widget_set_tooltip_text(GTK_WIDGET(hangupButton), _("Hang up"));
#endif
  gtk_widget_set_state( GTK_WIDGET(hangupButton), GTK_STATE_INSENSITIVE);
  g_signal_connect (G_OBJECT (hangupButton), "clicked",
      G_CALLBACK (hang_up), NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(ret), GTK_TOOL_ITEM(hangupButton), -1);  

  image = gtk_image_new_from_file( ICONS_DIR "/unhold.svg");
  unholdButton = gtk_tool_button_new (image, _("Off Hold"));
#if GTK_CHECK_VERSION(2,12,0)
  gtk_widget_set_tooltip_text(GTK_WIDGET(unholdButton), _("Off Hold"));
#endif
  gtk_widget_set_state( GTK_WIDGET(unholdButton), GTK_STATE_INSENSITIVE);
  g_signal_connect (G_OBJECT (unholdButton), "clicked",
      G_CALLBACK (unhold), NULL);
  //gtk_toolbar_insert(GTK_TOOLBAR(ret), GTK_TOOL_ITEM(unholdButton), -1);
  gtk_widget_show_all(GTK_WIDGET(unholdButton));

  image = gtk_image_new_from_file( ICONS_DIR "/hold.svg");
  holdButton =  gtk_tool_button_new (image, _("On Hold"));
#if GTK_CHECK_VERSION(2,12,0)
  gtk_widget_set_tooltip_text(GTK_WIDGET(holdButton), _("On Hold"));
#endif
  gtk_widget_set_state( GTK_WIDGET(holdButton), GTK_STATE_INSENSITIVE);
  g_signal_connect (G_OBJECT (holdButton), "clicked",
      G_CALLBACK (hold), NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(ret), GTK_TOOL_ITEM(holdButton), -1);

  image = gtk_image_new_from_file( ICONS_DIR "/transfert.svg");
  transfertButton = gtk_toggle_tool_button_new ();
  gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(transfertButton), image);
#if GTK_CHECK_VERSION(2,12,0)
  gtk_widget_set_tooltip_text(GTK_WIDGET(transfertButton), _("Transfer"));
#endif
  gtk_tool_button_set_label(GTK_TOOL_BUTTON(transfertButton), _("Transfer"));
  gtk_widget_set_state( GTK_WIDGET(transfertButton), GTK_STATE_INSENSITIVE);
  transfertButtonConnId = g_signal_connect (G_OBJECT (transfertButton), "toggled",
      G_CALLBACK (transfert), NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(ret), GTK_TOOL_ITEM(transfertButton), -1);  

  image = gtk_image_new_from_file( ICONS_DIR "/history2.svg");
  historyButton = gtk_toggle_tool_button_new();
  gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(historyButton), image);
#if GTK_CHECK_VERSION(2,12,0)
  gtk_widget_set_tooltip_text(GTK_WIDGET(historyButton), _("History"));
#endif
  gtk_tool_button_set_label(GTK_TOOL_BUTTON(historyButton), _("History"));
  g_signal_connect (G_OBJECT (historyButton), "toggled",
      G_CALLBACK (toggle_history), NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(ret), GTK_TOOL_ITEM(historyButton), -1);  
  history_shown = FALSE;
  active_calltree = current_calls;

  image = gtk_image_new_from_file( ICONS_DIR "/mailbox.svg");
  mailboxButton = gtk_tool_button_new( image , _("Voicemail"));
  gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(mailboxButton), image);
  if( account_list_get_size() ==0 ) gtk_widget_set_state( GTK_WIDGET(mailboxButton), GTK_STATE_INSENSITIVE );
#if GTK_CHECK_VERSION(2,12,0)
  gtk_widget_set_tooltip_text(GTK_WIDGET(mailboxButton), _("Voicemail"));
#endif
  g_signal_connect (G_OBJECT (mailboxButton), "clicked",
      G_CALLBACK (call_mailbox), NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(ret), GTK_TOOL_ITEM(mailboxButton), -1);

  return ret;

}  
static gboolean
on_key_released (GtkWidget   *widget,
                GdkEventKey *event,
                gpointer     user_data)  
{
  // If a modifier key is pressed, it's a shortcut, pass along
  if(event->state & GDK_CONTROL_MASK || 
     event->state & GDK_MOD1_MASK    ||
     event->keyval == 60             || // <
     event->keyval == 62             || // >
     event->keyval == 34             || // "
     event->keyval == 65361          || // left arrow
     event->keyval == 65363          || // right arrow
     event->keyval >= 65470          || // F-keys
     event->keyval == 32                // space
     )
    return FALSE;
  else
    sflphone_keypad(event->keyval, event->string);
  return TRUE;
}

  void 
create_call_tree (calltab_t* tab)
{
  GtkWidget *sw;
  GtkCellRenderer *rend;
  GtkTreeViewColumn *col;
  GtkTreeSelection *sel;

  tab->tree = gtk_vbox_new(FALSE, 10); 

  gtk_container_set_border_width (GTK_CONTAINER (tab->tree), 0);

  sw = gtk_scrolled_window_new( NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sw), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(sw), GTK_SHADOW_IN);
  g_signal_connect (G_OBJECT ( sw ), "key-press-event",G_CALLBACK (on_key_released), NULL);

  tab->store = gtk_list_store_new (3, 
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

  // Connect the popup menu
  g_signal_connect (G_OBJECT (tab->view), "popup-menu",
      G_CALLBACK (popup_menu), 
      NULL);
  g_signal_connect (G_OBJECT (tab->view), "button-press-event",
      G_CALLBACK (button_pressed), 
      NULL);


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

  gtk_widget_show(tab->tree); 

  //toolbar_update_buttons();

}

  void 
update_call_tree_remove (calltab_t* tab, call_t * c)
{
  GtkTreeIter iter;
  GValue val;
  call_t * iterCall;
  GtkListStore* store = tab->store;
  GtkWidget* view = tab->view;

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
  call_t * selectedCall = call_get_selected(tab);
  if(selectedCall == c)
    call_select(tab, NULL);
  toolbar_update_buttons();
}

  void 
update_call_tree (calltab_t* tab, call_t * c)
{
  GdkPixbuf *pixbuf;
  GtkTreeIter iter;
  GValue val;
  call_t * iterCall;
  GtkListStore* store = tab->store;
  GtkWidget* view = tab->view;

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
	gchar * description;
	gchar * date="";
	gchar * duration="";
	if(c->state == CALL_STATE_TRANSFERT)
	{
	  description = g_markup_printf_escaped("<b>%s</b> <i>%s</i>\n<i>Transfert to:</i> %s",  
	      call_get_number(c), 
	      call_get_name(c), 
	      c->to);
	}
	else
	{
	  description = g_markup_printf_escaped("<b>%s</b> <i>%s</i>", 
	      call_get_number(c),
	      call_get_name(c)); 
	}

	if( tab == current_calls )
	{
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
	}
	else
	{
	  switch(c->history_state)
	  {
	    case OUTGOING:
	      g_print("Outgoing state\n");
	      pixbuf = gdk_pixbuf_new_from_file( ICONS_DIR "/outgoing.svg", NULL);
	      break;
	    case INCOMING:
	      g_print("Incoming state\n");
	      pixbuf = gdk_pixbuf_new_from_file( ICONS_DIR "/incoming.svg", NULL);
	      break;
	    case MISSED:
	      g_print("Missed state\n");
	      pixbuf = gdk_pixbuf_new_from_file( ICONS_DIR "/missed.svg", NULL);
	      break;
	    default:
	      g_print("No history state\n");  
	      break;
	  }
	  date = timestamp_get_call_date(); 
	  duration = process_call_duration(c);
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
	gtk_list_store_set(store, &iter,
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

  void 
update_call_tree_add (calltab_t* tab, call_t * c)
{
  if( tab == history && ( call_list_get_size( tab ) > dbus_get_max_calls() ) )
    return;

  GdkPixbuf *pixbuf;
  GtkTreeIter iter;
  GtkTreeSelection* sel;

  // New call in the list
  gchar * description;
  gchar * date="";
  gchar * duration="";
  description = g_markup_printf_escaped("<b>%s</b> <i>%s</i>", 
      call_get_number(c),
      call_get_name(c)); 


  gtk_list_store_prepend (tab->store, &iter);

  if( tab == current_calls )
  {
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
  }
  else{
    switch(c->history_state)
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
	g_warning("History - Should not happen!");
    }
    date = timestamp_get_call_date(); 
    description = g_strconcat( date , description , NULL);
  }

  //Resize it
  if(pixbuf)
  {
    if(gdk_pixbuf_get_width(pixbuf) > 32 || gdk_pixbuf_get_height(pixbuf) > 32)
    {
      pixbuf =  gdk_pixbuf_scale_simple(pixbuf, 32, 32, GDK_INTERP_BILINEAR);
    }
  }
  gtk_list_store_set(tab->store, &iter,
      0, pixbuf, // Icon
      1, description, // Description
      2, c,      // Pointer
      -1);

  if (pixbuf != NULL)
    g_object_unref(G_OBJECT(pixbuf));

  sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(tab->view));
  gtk_tree_selection_select_iter(GTK_TREE_SELECTION(sel), &iter);
  toolbar_update_buttons();
}
