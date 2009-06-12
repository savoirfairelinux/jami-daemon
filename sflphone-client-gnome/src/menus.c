/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
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

#include <menus.h>
#include <config.h>
#include <configwindow.h>
#include <dbus/dbus.h>
#include <mainwindow.h>
#include <assistant.h>
#include <gtk/gtk.h>
#include <glib/gprintf.h>
#include <string.h> // for strlen

GtkWidget * pickUpMenu;
GtkWidget * hangUpMenu;
GtkWidget * newCallMenu;
GtkWidget * holdMenu;
GtkWidget * copyMenu;
GtkWidget * pasteMenu;
GtkWidget * recordMenu;

GtkWidget * editable_num;
GtkDialog * edit_dialog;

guint holdConnId;     //The hold_menu signal connection ID

GtkWidget * dialpadMenu;
GtkWidget * volumeMenu;
GtkWidget * searchbarMenu;


void update_menus()
{
    //Block signals for holdMenu
    gtk_signal_handler_block(GTK_OBJECT(holdMenu), holdConnId);

    gtk_widget_set_sensitive( GTK_WIDGET(pickUpMenu), FALSE);
    gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), FALSE);
    gtk_widget_set_sensitive( GTK_WIDGET(newCallMenu),FALSE);
    gtk_widget_set_sensitive( GTK_WIDGET(holdMenu),   FALSE);
    gtk_widget_set_sensitive( GTK_WIDGET(recordMenu), FALSE);
    gtk_widget_set_sensitive( GTK_WIDGET(copyMenu),   FALSE);

    callable_obj_t * selectedCall = calltab_get_selected_call(active_calltree);
    if (selectedCall)
    {
        gtk_widget_set_sensitive( GTK_WIDGET(copyMenu),   TRUE);
        switch(selectedCall->_state)
        {
            case CALL_STATE_INCOMING:
                gtk_widget_set_sensitive( GTK_WIDGET(pickUpMenu), TRUE);
                gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), TRUE);
                break;
            case CALL_STATE_HOLD:
                gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), TRUE);
                gtk_widget_set_sensitive( GTK_WIDGET(holdMenu),   TRUE);
                gtk_widget_set_sensitive( GTK_WIDGET(newCallMenu),TRUE);
                gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( holdMenu ), gtk_image_new_from_file( ICONS_DIR "/icon_unhold.svg"));
                break;
            case CALL_STATE_RINGING:
                gtk_widget_set_sensitive( GTK_WIDGET(pickUpMenu), TRUE);
                gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), TRUE);
                break;
            case CALL_STATE_DIALING:
                gtk_widget_set_sensitive( GTK_WIDGET(pickUpMenu), TRUE);
                gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), TRUE);
                gtk_widget_set_sensitive( GTK_WIDGET(newCallMenu),TRUE);
                break;
            case CALL_STATE_CURRENT:
            case CALL_STATE_RECORD:
                gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), TRUE);
                gtk_widget_set_sensitive( GTK_WIDGET(holdMenu),   TRUE);
                gtk_widget_set_sensitive( GTK_WIDGET(newCallMenu),TRUE);
                gtk_widget_set_sensitive( GTK_WIDGET(recordMenu), TRUE);
                gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( holdMenu ), gtk_image_new_from_file( ICONS_DIR "/icon_hold.svg"));
                break;
            case CALL_STATE_BUSY:
            case CALL_STATE_FAILURE:
                gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), TRUE);
                break;
            default:
                WARN("Should not happen in update_menus()!");
                break;
        }
    }
    else
    {
        gtk_widget_set_sensitive( GTK_WIDGET(newCallMenu), TRUE);
    }
    gtk_signal_handler_unblock(holdMenu, holdConnId);

}
/* ----------------------------------------------------------------- */
    static void
help_about ( void * foo UNUSED)
{
  gchar *authors[] = {
    "Yan Morin <yan.morin@savoirfairelinux.com>",
    "Jérôme Oufella <jerome.oufella@savoirfairelinux.com>",
    "Julien Plissonneau Duquene <julien.plissonneau.duquene@savoirfairelinux.com>",
    "Alexandre Bourget <alexandre.bourget@savoirfairelinux.com>",
    "Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>",
    "Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>",
    "Yun Liu <yun.liu@savoirfairelinux.com>",
    "Alexandre Savard <alexandre.savard@savoirfairelinux.com>",
    "Jean-Philippe Barrette-LaPierre",
    "Laurielle Lea",
    NULL};
  gchar *artists[] = {
    "Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>",
    "Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>",
    NULL};

  gtk_show_about_dialog( GTK_WINDOW(get_main_window()),
      "artists", artists,
      "authors", authors,
      "comments", _("SFLphone is a VoIP client compatible with SIP and IAX2 protocols."),
      "copyright", "Copyright © 2004-2009 Savoir-faire Linux Inc.",
      "name", PACKAGE,
      "title", _("About SFLphone"),
      "version", VERSION,
      "website", "http://www.sflphone.org",
      NULL);

}


    GtkWidget *
create_help_menu()
{
    GtkWidget * menu;
    GtkWidget * root_menu;
    GtkWidget * menu_items;

    menu      = gtk_menu_new ();

    menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_ABOUT, get_accel_group());
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
            G_CALLBACK (help_about),
            NULL);
    gtk_widget_show (menu_items);

    root_menu = gtk_menu_item_new_with_mnemonic (_("_Help"));
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (root_menu), menu);

    return root_menu;
}
/* ----------------------------------------------------------------- */
    GtkWidget *
create_waiting_icon()
{
    GtkWidget * waiting_icon;
    waiting_icon = gtk_image_menu_item_new_with_label("");
    gtk_image_menu_item_set_image (
            GTK_IMAGE_MENU_ITEM(waiting_icon),
            gtk_image_new_from_animation(
                gdk_pixbuf_animation_new_from_file(ICONS_DIR "/wait-on.gif", NULL)));
    gtk_menu_item_set_right_justified(GTK_MENU_ITEM(waiting_icon),TRUE);

    return waiting_icon;
}
/* ----------------------------------------------------------------- */
    static void
call_new_call ( void * foo UNUSED)
{
    sflphone_new_call();
}

    static void
call_quit ( void * foo UNUSED)
{
    sflphone_quit();
}

    static void
call_minimize ( void * foo UNUSED)
{
#if GTK_CHECK_VERSION(2,10,0)
    gtk_widget_hide(GTK_WIDGET( get_main_window() ));
    set_minimized( TRUE );
#endif
}

    static void
switch_account(  GtkWidget* item , gpointer data UNUSED)
{
    account_t* acc = g_object_get_data( G_OBJECT(item) , "account" );
    DEBUG("%s" , acc->accountID);
    account_list_set_current (acc);
    status_bar_display_account ();
}

    static void
call_hold  (void* foo UNUSED)
{
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls);

    if(selectedCall)
    {
        if(selectedCall->_state == CALL_STATE_HOLD)
        {
            gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( holdMenu ), gtk_image_new_from_file( ICONS_DIR "/icon_unhold.svg"));
            sflphone_off_hold();
        }
        else
        {
            gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( holdMenu ), gtk_image_new_from_file( ICONS_DIR "/icon_hold.svg"));
            sflphone_on_hold();
        }
    }
}

    static void
call_pick_up ( void * foo UNUSED)
{
    sflphone_pick_up();
}

    static void
call_hang_up ( void * foo UNUSED)
{
    sflphone_hang_up();
}

    static void
call_record ( void * foo UNUSED)
{
    sflphone_rec_call();
}

    static void
call_wizard ( void * foo UNUSED)
{
#if GTK_CHECK_VERSION(2,10,0)
    build_wizard();
#endif
}

    static void
remove_from_history( void * foo UNUSED)
{
    callable_obj_t* c = calltab_get_selected_call( history );
    if(c){
        DEBUG("Remove the call from the history");
        calllist_remove_from_history( c );
    }
}

    static void
call_back( void * foo UNUSED)
{
    callable_obj_t *selected_call, *new_call;
    
    selected_call = calltab_get_selected_call( active_calltree );

    if( selected_call )
    {
        create_new_call (CALL, CALL_STATE_DIALING, "", "", selected_call->_peer_name, selected_call->_peer_number, &new_call);

        calllist_add(current_calls, new_call);
        calltree_add_call(current_calls, new_call);
        sflphone_place_call(new_call);
        calltree_display (current_calls);
    }
}

    GtkWidget *
create_call_menu()
{
    GtkWidget * menu;
    GtkWidget * root_menu;
    GtkWidget * menu_items;
    GtkWidget * image;

    menu      = gtk_menu_new ();

    image = gtk_image_new_from_file( ICONS_DIR "/icon_call.svg");
    newCallMenu = gtk_image_menu_item_new_with_mnemonic(_("_New call"));
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( newCallMenu ), image );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), newCallMenu);
    g_signal_connect_swapped (G_OBJECT (newCallMenu), "activate",
            G_CALLBACK (call_new_call),
            NULL);
    gtk_widget_show (newCallMenu);

    menu_items = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);

    image = gtk_image_new_from_file( ICONS_DIR "/icon_accept.svg");
    pickUpMenu = gtk_image_menu_item_new_with_mnemonic(_("_Pick up"));
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( pickUpMenu ), image );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), pickUpMenu);
    gtk_widget_set_sensitive( GTK_WIDGET(pickUpMenu), FALSE);
    g_signal_connect_swapped (G_OBJECT (pickUpMenu), "activate",
            G_CALLBACK (call_pick_up),
            NULL);
    gtk_widget_show (pickUpMenu);

    image = gtk_image_new_from_file( ICONS_DIR "/icon_hangup.svg");
    hangUpMenu = gtk_image_menu_item_new_with_mnemonic(_("_Hang up"));
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( hangUpMenu ), image );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), hangUpMenu);
    gtk_widget_set_sensitive( GTK_WIDGET(hangUpMenu), FALSE);
    g_signal_connect_swapped (G_OBJECT (hangUpMenu), "activate",
            G_CALLBACK (call_hang_up),
            NULL);
    gtk_widget_show (hangUpMenu);

    image = gtk_image_new_from_file( ICONS_DIR "/icon_hold.svg");
    holdMenu = gtk_image_menu_item_new_with_mnemonic (_("On _Hold"));
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( holdMenu ), image );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), holdMenu);
    gtk_widget_set_sensitive( GTK_WIDGET(holdMenu),   FALSE);
    //Here we connect only to activate
    //The toggled state is managed from update_menus()
    holdConnId = g_signal_connect(G_OBJECT (holdMenu), "activate",
            G_CALLBACK (call_hold),
            NULL);
    gtk_widget_show (menu_items);

    image = gtk_image_new_from_stock (GTK_STOCK_MEDIA_RECORD, GTK_ICON_SIZE_MENU);
    recordMenu = gtk_image_menu_item_new_with_mnemonic(_("_Record"));
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( recordMenu ), image );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), recordMenu);
    gtk_widget_set_sensitive( GTK_WIDGET(recordMenu), FALSE);
    g_signal_connect_swapped (G_OBJECT (recordMenu), "activate",
            G_CALLBACK (call_record),
            NULL);
    gtk_widget_show (recordMenu);

    // Separator
    menu_items = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);

#if GTK_CHECK_VERSION(2,10,0)
    menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Account creation wizard"));
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    g_signal_connect_swapped( G_OBJECT( menu_items ) , "activate" , G_CALLBACK( call_wizard  ) , NULL );
    gtk_widget_show (menu_items);
    // Separator
    menu_items = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
#endif

    // Close menu to minimize the main window to the system tray
    menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_CLOSE, get_accel_group());
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
            G_CALLBACK (call_minimize),
            NULL);
    gtk_widget_show (menu_items);

    // Separator
    menu_items = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);

    // Quit Menu - quit SFLphone
    menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_QUIT, get_accel_group());
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
            G_CALLBACK (call_quit),
            NULL);
    gtk_widget_show (menu_items);


    root_menu = gtk_menu_item_new_with_mnemonic (_("_Call"));
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (root_menu), menu);

    return root_menu;
}
/* ----------------------------------------------------------------- */

    static void
edit_preferences ( void * foo UNUSED)
{
    show_config_window();
}

    static void
edit_accounts ( void * foo UNUSED)
{
    show_accounts_window();
}

// The menu Edit/Copy should copy the current selected call's number
    static void
edit_copy ( void * foo UNUSED)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls);
    gchar * no = NULL;

    if(selectedCall)
    {
        switch(selectedCall->_state)
        {
            case CALL_STATE_TRANSFERT:
            case CALL_STATE_DIALING:
            case CALL_STATE_RINGING:
                no = selectedCall->_peer_number;
                break;
            case CALL_STATE_CURRENT:
            case CALL_STATE_HOLD:
            case CALL_STATE_BUSY:
            case CALL_STATE_FAILURE:
            case CALL_STATE_INCOMING:
            default:
                no = selectedCall->_peer_number;
                break;
        }
	DEBUG("Clipboard number: %s\n", no);
        gtk_clipboard_set_text (clip, no, strlen(no) );
    }

}

// The menu Edit/Paste should paste the clipboard into the current selected call
    static void
edit_paste ( void * foo UNUSED)
{
    GtkClipboard* clip = gtk_clipboard_get(GDK_SELECTION_CLIPBOARD);
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls);
    gchar * no = gtk_clipboard_wait_for_text (clip);

    if(no && selectedCall)
    {
        switch(selectedCall->_state)
        {
            case CALL_STATE_TRANSFERT:
            case CALL_STATE_DIALING:
                // Add the text to the number
                {
		    gchar * before;
		    before = selectedCall->_peer_number;
		    DEBUG("TO: %s\n", before);
                    selectedCall->_peer_number = g_strconcat(before, no, NULL);

                    if(selectedCall->_state == CALL_STATE_DIALING)
                    {
                        selectedCall->_peer_info = g_strconcat("\"\" <", selectedCall->_peer_number, ">", NULL);	        		
                    }
                    calltree_update_call(current_calls, selectedCall);
                }
                break;
            case CALL_STATE_RINGING:
            case CALL_STATE_INCOMING:
            case CALL_STATE_BUSY:
            case CALL_STATE_FAILURE:
            case CALL_STATE_HOLD:
                { // Create a new call to hold the new text
                    selectedCall = sflphone_new_call();

                    gchar * before = selectedCall->_peer_number;
                    selectedCall->_peer_number = g_strconcat(selectedCall->_peer_number, no, NULL);
                    DEBUG("TO: %s", selectedCall->_peer_number);

                    selectedCall->_peer_info = g_strconcat("\"\" <", selectedCall->_peer_number, ">", NULL);

                    calltree_update_call(current_calls, selectedCall);
                }
                break;
            case CALL_STATE_CURRENT:
            default:
                {
                    unsigned int i;
                    for(i = 0; i < strlen(no); i++)
                    {
                        gchar * oneNo = g_strndup(&no[i], 1);
                        DEBUG("<%s>", oneNo);
                        dbus_play_dtmf(oneNo);

                        gchar * temp = g_strconcat(selectedCall->_peer_number, oneNo, NULL);
                        selectedCall->_peer_info = get_peer_info (temp, selectedCall->_peer_name);
                        // g_free(temp);
                        calltree_update_call(current_calls, selectedCall);

                    }
                }
                break;
        }

    }
    else // There is no current call, create one
    {
        selectedCall = sflphone_new_call();

        gchar * before = selectedCall->_peer_number;
        selectedCall->_peer_number = g_strconcat(selectedCall->_peer_number, no, NULL);
        g_free(before);
        DEBUG("TO: %s", selectedCall->_peer_number);

        g_free(selectedCall->_peer_info);
        selectedCall->_peer_info = g_strconcat("\"\" <", selectedCall->_peer_number, ">", NULL);
        calltree_update_call(current_calls,selectedCall);
    }

}

    static void
clear_history (void)
{
    if( calllist_get_size( history ) != 0 ){
        calllist_clean_history();
    }
}

    GtkWidget *
create_edit_menu()
{
    GtkWidget * menu;
    GtkWidget * image;
    GtkWidget * root_menu;
    GtkWidget * menu_items;

    menu      = gtk_menu_new ();

    copyMenu = gtk_image_menu_item_new_from_stock( GTK_STOCK_COPY, get_accel_group());
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), copyMenu);
    g_signal_connect_swapped (G_OBJECT (copyMenu), "activate",
            G_CALLBACK (edit_copy),
            NULL);
    gtk_widget_show (copyMenu);

    pasteMenu = gtk_image_menu_item_new_from_stock( GTK_STOCK_PASTE, get_accel_group());
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), pasteMenu);
    g_signal_connect_swapped (G_OBJECT (pasteMenu), "activate",
            G_CALLBACK (edit_paste),
            NULL);
    gtk_widget_show (pasteMenu);

    menu_items = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);

    menu_items = gtk_image_menu_item_new_with_mnemonic(_("Clear _history"));
    image = gtk_image_new_from_stock( GTK_STOCK_CLEAR , GTK_ICON_SIZE_MENU );
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( menu_items ), image );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
            G_CALLBACK (clear_history),
            NULL);
    gtk_widget_show (menu_items);

    menu_items = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);

    menu_items = gtk_menu_item_new_with_mnemonic( _("_Accounts") );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
            G_CALLBACK (edit_accounts),
            NULL);
    gtk_widget_show (menu_items);

    menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_PREFERENCES, NULL);
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    g_signal_connect_swapped (G_OBJECT (menu_items), "activate",
            G_CALLBACK (edit_preferences),
            NULL);
    gtk_widget_show (menu_items);


    root_menu = gtk_menu_item_new_with_mnemonic (_("_Edit"));
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (root_menu), menu);

    return root_menu;
}
/* ----------------------------------------------------------------- */
    static void
view_dialpad  (GtkImageMenuItem *imagemenuitem UNUSED,
        void* foo UNUSED)
{
    gboolean state;
    main_window_dialpad( &state );
    if( state )
        gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( dialpadMenu ),
                gtk_image_new_from_file( ICONS_DIR "/icon_dialpad_off.svg"));
    else
        gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( dialpadMenu ),
                gtk_image_new_from_file( ICONS_DIR "/icon_dialpad.svg"));
    dbus_set_dialpad( state );


}

    static void
view_volume_controls  (GtkImageMenuItem *imagemenuitem UNUSED,
        void* foo UNUSED)
{
    gboolean state;
    main_window_volume_controls( &state );
    if( state )
        gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( volumeMenu ),
                gtk_image_new_from_file( ICONS_DIR "/icon_volume_off.svg"));
    else
        gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( volumeMenu ),
                gtk_image_new_from_file( ICONS_DIR "/icon_volume.svg"));
    dbus_set_volume_controls( state );
}

/*
   static void
   view_searchbar  (GtkImageMenuItem *imagemenuitem UNUSED,
   void* foo UNUSED)
   {
   gboolean state;
   main_window_searchbar( &state );
   dbus_set_searchbar( state );
   }
   */
    GtkWidget *
create_view_menu()
{
    GtkWidget * menu;
    GtkWidget * root_menu;
    GtkWidget * image;

    menu      = gtk_menu_new ();

    if( SHOW_DIALPAD )
        image = gtk_image_new_from_file( ICONS_DIR "/icon_dialpad_off.svg");
    else
        image = gtk_image_new_from_file( ICONS_DIR "/icon_dialpad.svg");
    dialpadMenu = gtk_image_menu_item_new_with_mnemonic (_("_Dialpad"));
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( dialpadMenu ), image );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), dialpadMenu);
    g_signal_connect(G_OBJECT ( dialpadMenu ), "activate",
            G_CALLBACK (view_dialpad),
            NULL);
    gtk_widget_show (dialpadMenu);

    if( SHOW_VOLUME )
        image = gtk_image_new_from_file( ICONS_DIR "/icon_volume.svg");
    else
        image = gtk_image_new_from_file( ICONS_DIR "/icon_volume.svg");
    volumeMenu = gtk_image_menu_item_new_with_mnemonic (_("_Volume controls"));
    gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( volumeMenu ), image );
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), volumeMenu);
    g_signal_connect(G_OBJECT (volumeMenu), "activate",
            G_CALLBACK (view_volume_controls),
            NULL);
    gtk_widget_show (volumeMenu);

    /*image = gtk_image_new_from_stock( GTK_STOCK_FIND , GTK_ICON_SIZE_MENU );
      searchbarMenu = gtk_image_menu_item_new_with_mnemonic (_("_Search bar"));
      gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( searchbarMenu ), image );
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), searchbarMenu);
      g_signal_connect(G_OBJECT (searchbarMenu), "activate",
      G_CALLBACK (view_searchbar),
      NULL);
      gtk_widget_show (searchbarMenu);
      */

    root_menu = gtk_menu_item_new_with_mnemonic (_("_View"));
    gtk_menu_item_set_submenu (GTK_MENU_ITEM (root_menu), menu);

    return root_menu;
}
/* ----------------------------------------------------------------- */
    GtkWidget *
create_menus ( )
{

    GtkWidget * menu_bar;
    GtkWidget * root_menu;

    menu_bar  = gtk_menu_bar_new ();

    root_menu = create_call_menu();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), root_menu);

    root_menu = create_edit_menu();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), root_menu);

    root_menu = create_view_menu();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), root_menu);

    root_menu = create_help_menu();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu_bar), root_menu);

    // Create waiting icon
    waitingLayer = create_waiting_icon();
    gtk_menu_shell_append(GTK_MENU_SHELL (menu_bar),waitingLayer);

    return menu_bar;
}

/* ----------------------------------------------------------------- */

static void edit_number_cb (GtkWidget *widget UNUSED, gpointer user_data) {

    show_edit_number ((callable_obj_t*)user_data);
}


    void
show_popup_menu (GtkWidget *my_widget, GdkEventButton *event)
{
    // TODO update the selection to make sure the call under the mouse is the call selected

    gboolean pickup = FALSE, hangup = FALSE, hold = FALSE, copy = FALSE, record = FALSE;
    gboolean accounts = FALSE;

    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls);
    if (selectedCall)
    {
        copy = TRUE;
        switch(selectedCall->_state)
        {
            case CALL_STATE_INCOMING:
                pickup = TRUE;
                hangup = TRUE;
                break;
            case CALL_STATE_HOLD:
                hangup = TRUE;
                hold   = TRUE;
                break;
            case CALL_STATE_RINGING:
                hangup = TRUE;
                break;
            case CALL_STATE_DIALING:
                pickup = TRUE;
                hangup = TRUE;
                accounts = TRUE;
                break;
            case CALL_STATE_RECORD:
            case CALL_STATE_CURRENT:
                hangup = TRUE;
                hold   = TRUE;
                record = TRUE;
                break;
            case CALL_STATE_BUSY:
            case CALL_STATE_FAILURE:
                hangup = TRUE;
                break;
            default:
                WARN("Should not happen in show_popup_menu!");
                break;
        }
    }

    GtkWidget *menu;
    GtkWidget *image;
    int button, event_time;
    GtkWidget * menu_items;

    menu = gtk_menu_new ();
    //g_signal_connect (menu, "deactivate",
    //       G_CALLBACK (gtk_widget_destroy), NULL);

    if(copy)
    {
        menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_COPY, get_accel_group());
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate",
                G_CALLBACK (edit_copy),
                NULL);
        gtk_widget_show (menu_items);
    }

    menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_PASTE, get_accel_group());
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    g_signal_connect (G_OBJECT (menu_items), "activate",
            G_CALLBACK (edit_paste),
            NULL);
    gtk_widget_show (menu_items);

    if(pickup || hangup || hold)
    {
        menu_items = gtk_separator_menu_item_new ();
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        gtk_widget_show (menu_items);
    }

    if(pickup)
    {

        menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Pick up"));
        image = gtk_image_new_from_file( ICONS_DIR "/icon_accept.svg");
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_items), image);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate",
                G_CALLBACK (call_pick_up),
                NULL);
        gtk_widget_show (menu_items);
    }

    if(hangup)
    {
        menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Hang up"));
        image = gtk_image_new_from_file( ICONS_DIR "/icon_hangup.svg");
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_items), image);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate",
                G_CALLBACK (call_hang_up),
                NULL);
        gtk_widget_show (menu_items);
    }

    if(hold)
    {
        menu_items = gtk_check_menu_item_new_with_mnemonic (_("On _Hold"));
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items),
                (selectedCall->_state == CALL_STATE_HOLD ? TRUE : FALSE));
        g_signal_connect(G_OBJECT (menu_items), "activate",
                G_CALLBACK (call_hold),
                NULL);
        gtk_widget_show (menu_items);
    }

    if(record)
    {
        menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Record"));
        image = gtk_image_new_from_stock (GTK_STOCK_MEDIA_RECORD, GTK_ICON_SIZE_MENU);
        gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(menu_items), image);
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate",
                G_CALLBACK (call_record),
                NULL);
        gtk_widget_show (menu_items);
    }

    if(accounts)
    {
        add_registered_accounts_to_menu (menu);
    }

    if (event)
    {
        button = event->button;
        event_time = event->time;
    }
    else
    {
        button = 0;
        event_time = gtk_get_current_event_time ();
    }

    gtk_menu_attach_to_widget (GTK_MENU (menu), my_widget, NULL);
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
            button, event_time);
}


    void
show_popup_menu_history(GtkWidget *my_widget, GdkEventButton *event)
{

    gboolean pickup = FALSE;
    gboolean remove = FALSE;
    gboolean edit = FALSE;

    callable_obj_t * selectedCall = calltab_get_selected_call( history );
    if (selectedCall)
    {
        remove = TRUE;
        pickup = TRUE;
        edit = TRUE;
    }

    GtkWidget *menu;
    GtkWidget *image;
    int button, event_time;
    GtkWidget * menu_items;

    menu = gtk_menu_new ();
    //g_signal_connect (menu, "deactivate",
    //       G_CALLBACK (gtk_widget_destroy), NULL);

    if(pickup)
    {

        menu_items = gtk_image_menu_item_new_with_mnemonic(_("_Call back"));
        image = gtk_image_new_from_file( ICONS_DIR "/icon_accept.svg");
        gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( menu_items ), image );
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate",G_CALLBACK (call_back), NULL);
        gtk_widget_show (menu_items);
    }

    menu_items = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    gtk_widget_show (menu_items);

    if (edit)
    {
        menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_EDIT, get_accel_group());
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate",G_CALLBACK (edit_number_cb), selectedCall);
        gtk_widget_show (menu_items);
    }


    if(remove)
    {
        menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_DELETE, get_accel_group());
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate", G_CALLBACK (remove_from_history),  NULL);
        gtk_widget_show (menu_items);
    }

    if (event)
    {
        button = event->button;
        event_time = event->time;
    }
    else
    {
        button = 0;
        event_time = gtk_get_current_event_time ();
    }

    gtk_menu_attach_to_widget (GTK_MENU (menu), my_widget, NULL);
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
            button, event_time);
}
    void
show_popup_menu_contacts(GtkWidget *my_widget, GdkEventButton *event)
{

    gboolean pickup = FALSE;
    gboolean accounts = FALSE;
    gboolean edit = FALSE;

    callable_obj_t * selectedCall = calltab_get_selected_call( contacts );
    if (selectedCall)
    {
        pickup = TRUE;
        accounts = TRUE;
        edit = TRUE;
    }

    GtkWidget *menu;
    GtkWidget *image;
    int button, event_time;
    GtkWidget * menu_items;

    menu = gtk_menu_new ();
    //g_signal_connect (menu, "deactivate",
    //       G_CALLBACK (gtk_widget_destroy), NULL);

    if(pickup)
    {

        menu_items = gtk_image_menu_item_new_with_mnemonic(_("_New call"));
        image = gtk_image_new_from_file( ICONS_DIR "/icon_accept.svg");
        gtk_image_menu_item_set_image( GTK_IMAGE_MENU_ITEM ( menu_items ), image );
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate",G_CALLBACK (call_back), NULL);
        gtk_widget_show (menu_items);
    }

    if (edit)
    {
        menu_items = gtk_image_menu_item_new_from_stock( GTK_STOCK_EDIT, get_accel_group());
        gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
        g_signal_connect (G_OBJECT (menu_items), "activate",G_CALLBACK (edit_number_cb), selectedCall);
        gtk_widget_show (menu_items);
    }

    if(accounts)
    {
        add_registered_accounts_to_menu (menu);
    }

    if (event)
    {
        button = event->button;
        event_time = event->time;
    }
    else
    {
        button = 0;
        event_time = gtk_get_current_event_time ();
    }

    gtk_menu_attach_to_widget (GTK_MENU (menu), my_widget, NULL);
    gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL,
            button, event_time);
}


void add_registered_accounts_to_menu (GtkWidget *menu) {

    GtkWidget *menu_items;
    unsigned int i;
    account_t* acc, *current;
    gchar* alias;

    menu_items = gtk_separator_menu_item_new ();
    gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
    gtk_widget_show (menu_items);

    for( i = 0 ; i < account_list_get_size() ; i++ ){
        acc = account_list_get_nth(i);
        // Display only the registered accounts
        if( g_strcasecmp( account_state_name(acc->state) , account_state_name(ACCOUNT_STATE_REGISTERED) ) == 0 ){
            alias = g_strconcat( g_hash_table_lookup(acc->properties , ACCOUNT_ALIAS) , " - ",g_hash_table_lookup(acc->properties , ACCOUNT_TYPE), NULL);
            menu_items = gtk_check_menu_item_new_with_mnemonic(alias);
            gtk_menu_shell_append (GTK_MENU_SHELL (menu), menu_items);
            g_object_set_data( G_OBJECT( menu_items ) , "account" , acc );
            g_free( alias );
            current = account_list_get_current();
            if(current){
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(menu_items),
                        (g_strcasecmp( acc->accountID , current->accountID) == 0)? TRUE : FALSE);
            }
            g_signal_connect (G_OBJECT (menu_items), "activate",
                    G_CALLBACK (switch_account),
                    NULL);
            gtk_widget_show (menu_items);
        } // fi
    }

}

static void ok_cb (GtkWidget *widget UNUSED, gpointer userdata) {

    gchar *new_number;
    callable_obj_t *modified_call, *original;

    // Change the number of the selected call before calling
    new_number = (gchar*) gtk_entry_get_text (GTK_ENTRY (editable_num));
    original = (callable_obj_t*)userdata;

    // Create the new call
    create_new_call (CALL, CALL_STATE_DIALING, "", g_strdup (original->_accountID), original->_peer_name, g_strdup (new_number), &modified_call);

    // Update the internal data structure and the GUI
    calllist_add(current_calls, modified_call);
    calltree_add_call(current_calls, modified_call);
    sflphone_place_call(modified_call);
    calltree_display (current_calls);

    // Close the contextual menu
    gtk_widget_destroy (GTK_WIDGET (edit_dialog));
}

static void on_delete (GtkWidget * widget)
{
    gtk_widget_destroy (widget);
}

void show_edit_number (callable_obj_t *call) {

    GtkWidget *ok, *hbox, *image;
    GdkPixbuf *pixbuf;

    edit_dialog = GTK_DIALOG (gtk_dialog_new());

    // Set window properties
    gtk_window_set_default_size(GTK_WINDOW(edit_dialog), 300, 20);
    gtk_window_set_title(GTK_WINDOW(edit_dialog), _("Edit phone"));
    gtk_window_set_resizable (GTK_WINDOW (edit_dialog), FALSE);

    g_signal_connect (G_OBJECT (edit_dialog), "delete-event", G_CALLBACK (on_delete), NULL);

    hbox = gtk_hbox_new (FALSE, 0);
    gtk_box_pack_start(GTK_BOX (edit_dialog->vbox), hbox, TRUE, TRUE, 0);

    // Set the number to be edited
    editable_num = gtk_entry_new ();
#if GTK_CHECK_VERSION(2,12,0)
    gtk_widget_set_tooltip_text(GTK_WIDGET(editable_num), _("Edit the phone number before making a call"));
#endif
    if (call)
        gtk_entry_set_text(GTK_ENTRY(editable_num), g_strdup (call->_peer_number));
    else
        ERROR ("This a bug, the call should be defined. menus.c line 1051");

    gtk_box_pack_start(GTK_BOX (hbox), editable_num, TRUE, TRUE, 0);

    // Set a custom image for the button
    pixbuf = gdk_pixbuf_new_from_file_at_scale (ICONS_DIR "/outgoing.svg", 32, 32, TRUE, NULL);
    image = gtk_image_new_from_pixbuf (pixbuf);
    ok = gtk_button_new ();
    gtk_button_set_image (GTK_BUTTON (ok), image);
    gtk_box_pack_start(GTK_BOX (hbox), ok, TRUE, TRUE, 0);
    g_signal_connect(G_OBJECT (ok), "clicked", G_CALLBACK (ok_cb), call);

    gtk_widget_show_all (edit_dialog->vbox);

    gtk_dialog_run(edit_dialog);

}


