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
#include <glib/gprintf.h>

#include <gtk/gtk.h>
#include <actions.h>
#include <calltree.h>
#include <calllist.h>
#include <menus.h>
#include <dbus.h>
#include <contactlist/eds.h>
#include "addressbook-config.h"

GtkWidget   * toolbar;
GtkToolItem * pickupButton;
GtkToolItem * callButton;
GtkToolItem * hangupButton;
GtkToolItem * holdButton;
GtkToolItem * transfertButton;
GtkToolItem * unholdButton;
GtkToolItem * mailboxButton;
GtkToolItem * recButton;
GtkToolItem * historyButton;
GtkToolItem * contactButton;

guint transfertButtonConnId; //The button toggled signal connection ID

void
free_call_t (call_t *c)
{
    g_free (c->callID);
    g_free (c->accountID);
    g_free (c->from);
    g_free (c->to);
    g_free (c);
}

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

  static gboolean
is_inserted( GtkWidget* button )
{
  return ( GTK_WIDGET(button)->parent == GTK_WIDGET( toolbar ) );
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
call_button( GtkWidget *widget UNUSED, gpointer   data UNUSED)
{
  call_t * selectedCall;
  call_t* new_call;
  gchar *to, *from;

  selectedCall = call_get_selected(active_calltree);
  
  if(call_list_get_size(current_calls)>0)
    sflphone_pick_up();
  
  else if(call_list_get_size(active_calltree) > 0){
    if( selectedCall)
    {
      printf("Calling a called num\n");

      to = g_strdup(call_get_number(selectedCall));
      from = g_strconcat("\"\" <", call_get_number(selectedCall), ">",NULL);

      create_new_call (to, from, CALL_STATE_DIALING, "", &new_call);

      printf("call : from : %s to %s\n", new_call->from, new_call->to);

      call_list_add(current_calls, new_call);
      update_call_tree_add(current_calls, new_call);
      sflphone_place_call(new_call);
      display_calltree (current_calls);
    }
    else
    {
      sflphone_new_call();
      display_calltree(current_calls);
    }
  }
  else
  {
    sflphone_new_call();
    display_calltree(current_calls);
  }
}

/**
 * Hang up the line
 */
  static void
hang_up( GtkWidget *widget UNUSED, gpointer   data UNUSED)
{
  sflphone_hang_up();
}

/**
 * Hold the line
 */
  static void
hold( GtkWidget *widget UNUSED, gpointer   data UNUSED)
{
  sflphone_on_hold();
}

/**
 * Transfert the line
 */
  static void
transfert  (GtkToggleToolButton *toggle_tool_button,
    gpointer             user_data UNUSED )
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
unhold( GtkWidget *widget UNUSED, gpointer   data UNUSED)
{
  sflphone_off_hold();
}

static void toggle_button_cb (GtkToggleToolButton *widget, gpointer user_data)
{
    calltab_t * to_switch;
    gboolean toggle;

    to_switch = (calltab_t*) user_data;
    toggle = gtk_toggle_tool_button_get_active (widget);

    g_print ("%i\n", toggle);

    (toggle)? display_calltree (to_switch) : display_calltree (current_calls);
}


void create_new_entry_in_contactlist (gchar *contact_name, gchar *contact_phone, contact_type_t type, GdkPixbuf *photo){
   
    gchar *from;
    call_t *new_call;
    GdkPixbuf *pixbuf;

    /* Check if the information is valid */
    if (g_strcasecmp (contact_phone, EMPTY_ENTRY) != 0){
        from = g_strconcat("\"" , contact_name, "\"<", contact_phone, ">", NULL);
        create_new_call (from, from, CALL_STATE_DIALING, "", &new_call);

        // Attach a pixbuf to a contact
        if (photo) {
            attach_thumbnail (new_call, photo);
        }
        else {
            switch (type) {
                case CONTACT_PHONE_BUSINESS:
                    pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/face-monkey.svg", NULL);
                    break;
                case CONTACT_PHONE_HOME:
                    pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/home.svg", NULL);
                    break;
                case CONTACT_PHONE_MOBILE:
                    pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/users.svg", NULL);
                    break;
                default:
                    pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/contact_default.svg", NULL);
                    break;
            }
            attach_thumbnail (new_call, pixbuf);
        }

        call_list_add (contacts, new_call);
        update_call_tree_add(contacts, new_call);
    }
}

  static void
call_mailbox( GtkWidget* widget UNUSED, gpointer data UNUSED)
{
    account_t* current;
    call_t *mailbox_call;
    gchar *to, *from, *account_id;

    current = account_list_get_current ();
    if( current == NULL ) // Should not happens
        return;
  
    to = g_strdup(g_hash_table_lookup(current->properties, ACCOUNT_MAILBOX));
    from = g_markup_printf_escaped(_("\"Voicemail\" <%s>"),  to);
    account_id = g_strdup (current->accountID);
  
    create_new_call (to, from, CALL_STATE_DIALING, account_id, &mailbox_call);
    g_print("TO : %s\n" , mailbox_call->to);
    call_list_add( current_calls , mailbox_call );
    update_call_tree_add( current_calls , mailbox_call );
    update_menus();
    sflphone_place_call( mailbox_call );
    display_calltree(current_calls);
}



/**
 * Static rec_button
 */
static void
rec_button( GtkWidget *widget UNUSED, gpointer   data UNUSED)
{
  sflphone_rec_call();
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
  gtk_widget_set_sensitive( GTK_WIDGET(recButton),        FALSE);
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
  //gtk_toolbar_insert(GTK_TOOLBAR(toolbar), recButton, 0);


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
	            if( active_calltree == current_calls )  gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     TRUE);
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
                gtk_widget_set_sensitive( GTK_WIDGET(recButton),        TRUE);
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
            case CALL_STATE_RECORD:
	            gtk_widget_set_sensitive( GTK_WIDGET(hangupButton),     TRUE);
	            gtk_widget_set_sensitive( GTK_WIDGET(holdButton),       TRUE);
	            gtk_widget_set_sensitive( GTK_WIDGET(transfertButton),  TRUE);
	            gtk_widget_set_sensitive( GTK_WIDGET(callButton),       TRUE);
                gtk_widget_set_sensitive( GTK_WIDGET(recButton),        TRUE);
	        break;
            default:
	            g_warning("Toolbar update - Should not happen!");
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
selected(GtkTreeSelection *sel, void* data UNUSED )
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
void  row_activated(GtkTreeView       *tree_view UNUSED,
    GtkTreePath       *path UNUSED,
    GtkTreeViewColumn *column UNUSED,
    void * data UNUSED)
{
    call_t* selectedCall;
    call_t* new_call;
    gchar *to, *from, *account_id;
    
    g_print("double click action\n");
  
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
	                g_warning("Row activated - Should not happen!");
	                break;
            }
        }
    
        // If history or contact: double click action places a new call
        else
        {
            to = g_strdup(call_get_number(selectedCall));
            from = g_strconcat("\"\" <", call_get_number(selectedCall), ">",NULL);
            account_id = g_strdup (selectedCall->accountID);

            // Create a new call
            create_new_call (to, from, CALL_STATE_DIALING, account_id, &new_call);

            call_list_add(current_calls, new_call);
            update_call_tree_add(current_calls, new_call);
            sflphone_place_call(new_call);
            display_calltree(current_calls);
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
  gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (historyButton), image);
#if GTK_CHECK_VERSION(2,12,0)
  gtk_widget_set_tooltip_text(GTK_WIDGET(historyButton), _("History"));
#endif
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (historyButton), _("History"));
  g_signal_connect (G_OBJECT (historyButton), "toggled", G_CALLBACK (toggle_button_cb), history);
  gtk_toolbar_insert(GTK_TOOLBAR(ret), GTK_TOOL_ITEM(historyButton), -1);
  active_calltree = current_calls;

  image = gtk_image_new_from_file( ICONS_DIR "/addressbook.svg");
  contactButton = gtk_toggle_tool_button_new();
  gtk_tool_button_set_icon_widget (GTK_TOOL_BUTTON (contactButton), image);
#if GTK_CHECK_VERSION(2,12,0)
  gtk_widget_set_tooltip_text(GTK_WIDGET(contactButton), _("Address book"));
#endif
  gtk_tool_button_set_label (GTK_TOOL_BUTTON (contactButton), _("Address book"));
  g_signal_connect (G_OBJECT (contactButton), "toggled", G_CALLBACK (toggle_button_cb), contacts);
  gtk_toolbar_insert(GTK_TOOLBAR(ret), GTK_TOOL_ITEM(contactButton), -1);

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

  recButton = gtk_tool_button_new_from_stock (GTK_STOCK_MEDIA_RECORD);
#if GTK_CHECK_VERSION(2,12,0)
  gtk_widget_set_tooltip_text(GTK_WIDGET(recButton), _("Record a call"));
#endif
  gtk_widget_set_state( GTK_WIDGET(recButton), GTK_STATE_INSENSITIVE);
  g_signal_connect (G_OBJECT (recButton), "clicked",
      G_CALLBACK (rec_button), NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(ret), GTK_TOOL_ITEM(recButton), -1);


  return ret;

}
static gboolean
on_key_released (GtkWidget   *widget UNUSED,
                GdkEventKey *event,
                gpointer     user_data UNUSED)
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

/**
 * Reset call tree
 */
  void
reset_call_tree (calltab_t* tab)
{
  gtk_list_store_clear (tab->store);
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
  g_signal_connect (G_OBJECT ( sw ), "key-release-event",G_CALLBACK (on_key_released), NULL);

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
    g_print("update call tree\n");
  GdkPixbuf *pixbuf=NULL;
  GtkTreeIter iter;
  GValue val;
  call_t * iterCall;
  GtkListStore* store = tab->store;

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
            case CALL_STATE_RECORD:
	      pixbuf = gdk_pixbuf_new_from_file(ICONS_DIR "/rec_call.svg", NULL);
	      break;
	    default:
	      g_warning("Update calltree - Should not happen!");
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

  GdkPixbuf *pixbuf=NULL;
  GtkTreeIter iter;
  GtkTreeSelection* sel;

  // New call in the list
  gchar * description;
  gchar * date="";
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
	g_warning("Update calltree add - Should not happen!");
    }
  }

  else if (tab == history) {
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

  else if (tab == contacts) {
    pixbuf = c->contact_thumbnail; 
    description = g_strconcat( description , NULL);
  }

  else {
        g_warning ("This widget doesn't exist - This is a bug in the application\n.");
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

void display_calltree (calltab_t *tab) {

    GtkTreeSelection *sel;

    g_print ("display_calltree called\n");

    /* If we already are displaying the specified calltree */
    if (active_calltree == tab)
        return;

    /* case 1: we want to display the main calltree */
    if (tab==current_calls) {

        g_print ("display main tab\n");
        
        if (active_calltree==contacts) {
            gtk_toggle_tool_button_set_active ((GtkToggleToolButton*)contactButton, FALSE);
        } else {
            gtk_toggle_tool_button_set_active ((GtkToggleToolButton*)historyButton, FALSE);
        }
    
    }
    
    /* case 2: we want to display the history */
    else if (tab==history) {
        
        g_print ("display history tab\n");

        if (active_calltree==contacts) {
            gtk_toggle_tool_button_set_active ((GtkToggleToolButton*)contactButton, FALSE);
        }

        gtk_toggle_tool_button_set_active ((GtkToggleToolButton*)historyButton, TRUE);
    }

    else if (tab==contacts) {
    
        g_print ("display contact tab\n");
        
        if (active_calltree==history) {
            gtk_toggle_tool_button_set_active ((GtkToggleToolButton*)historyButton, FALSE);
        }
        
        gtk_toggle_tool_button_set_active ((GtkToggleToolButton*)contactButton, TRUE);
    }

    else 
        g_print ("calltree.c line 1050 . This is probably a bug in the application\n");


    gtk_widget_hide (active_calltree->tree);
    active_calltree = tab;
    gtk_widget_show (active_calltree->tree);

	sel = gtk_tree_view_get_selection (GTK_TREE_VIEW (active_calltree->view));
	g_signal_emit_by_name(sel, "changed");
	toolbar_update_buttons();
	//gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(histfilter));
}




















