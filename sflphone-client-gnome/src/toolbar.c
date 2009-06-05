/*
 *  Copyright (C) 2009 Savoir-Faire Linux inc.
 *  Author: Julien Bonjean <julien.bonjean@savoirfairelinux.com>
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

#include <toolbar.h>
#include <contacts/addressbook.h>

    static gboolean
is_inserted( GtkWidget* button )
{
    return ( GTK_WIDGET(button)->parent == GTK_WIDGET( toolbar ) );
}

/**
 * Static rec_button
 */
    static void
rec_button( GtkWidget *widget UNUSED, gpointer   data UNUSED)
{
    sflphone_rec_call();
}

    static void
call_mailbox( GtkWidget* widget UNUSED, gpointer data UNUSED)
{
    account_t* current;
    callable_obj_t *mailbox_call;
    gchar *to, *from, *account_id;

    current = account_list_get_current ();
    if( current == NULL ) // Should not happens
        return;

    to = g_strdup(g_hash_table_lookup(current->properties, ACCOUNT_MAILBOX));
    from = g_markup_printf_escaped(_("\"Voicemail\" <%s>"),  to);
    account_id = g_strdup (current->accountID);

    create_new_call (CALL, CALL_STATE_DIALING, "", account_id, "Voicemail", to, &mailbox_call);
    DEBUG("TO : %s" , mailbox_call->_peer_number);
    calllist_add( current_calls , mailbox_call );
    calltree_add_call( current_calls , mailbox_call );
    update_menus();
    sflphone_place_call( mailbox_call );
    calltree_display(current_calls);
}

/**
 * Make a call
 */
    static void
call_button( GtkWidget *widget UNUSED, gpointer   data UNUSED)
{
    DEBUG("------ call_button -----");
    callable_obj_t * selectedCall;
    callable_obj_t* new_call;

    selectedCall = calltab_get_selected_call(active_calltree);

    if(calllist_get_size(current_calls)>0)
        sflphone_pick_up();

    else if(calllist_get_size(active_calltree) > 0){
        if( selectedCall)
        {
            create_new_call (CALL, CALL_STATE_DIALING, "", "", "", selectedCall->_peer_number, &new_call);

            calllist_add(current_calls, new_call);
            calltree_add_call(current_calls, new_call);
            sflphone_place_call(new_call);
            calltree_display (current_calls);
        }
        else
        {
            sflphone_new_call();
            calltree_display(current_calls);
        }
    }
    else
    {
        sflphone_new_call();
        calltree_display(current_calls);
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

    (toggle)? calltree_display (to_switch) : calltree_display (current_calls);
}

GtkWidget *create_toolbar ()
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
    gtk_widget_set_state( GTK_WIDGET(contactButton), GTK_STATE_INSENSITIVE);
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
    gtk_widget_set_sensitive( GTK_WIDGET(contactButton),        FALSE);
    g_object_ref (contactButton);
    if( is_inserted( GTK_WIDGET(contactButton) ) )     gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET (contactButton));
    g_object_ref(holdButton);
    g_object_ref(unholdButton);
    if( is_inserted( GTK_WIDGET(holdButton) ) )   gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(holdButton));
    if( is_inserted( GTK_WIDGET(unholdButton) ) ) gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(unholdButton));
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), holdButton, 3);
    g_object_ref(callButton);
    g_object_ref(pickupButton);
    if( is_inserted( GTK_WIDGET(callButton) ) ) gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(callButton));
    if( is_inserted( GTK_WIDGET(pickupButton) ) ) gtk_container_remove(GTK_CONTAINER(toolbar), GTK_WIDGET(pickupButton));
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), callButton, 0);

    // If addressbook support has been enabled and all addressbooks are loaded, display the icon
    if (addressbook_is_enabled () && addressbook_is_ready()) {  
        gtk_toolbar_insert(GTK_TOOLBAR(toolbar), contactButton, 5);
        // Make the icon clickable only if at least one address book is active
        if (addressbook_is_active ())   gtk_widget_set_sensitive( GTK_WIDGET(contactButton), TRUE);
    }

    gtk_signal_handler_block(GTK_OBJECT(transfertButton),transfertButtonConnId);
    gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(transfertButton), FALSE);
    gtk_signal_handler_unblock(transfertButton, transfertButtonConnId);

    callable_obj_t * selectedCall = calltab_get_selected_call(active_calltree);
    if (selectedCall)
    {
        switch(selectedCall->_state)
        {
            case CALL_STATE_INCOMING:
                gtk_widget_set_sensitive( GTK_WIDGET(pickupButton),     TRUE);
                gtk_widget_set_sensitive( GTK_WIDGET(hangupButton), TRUE);
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
                // Fix bug #1145
                // Actually it could happen when sflphone_fill_account_list()
                // call this function and no "call" is selected
                // WARN("Toolbar update - Should not happen!");
                break;
        }
    }
    else
    {
        if( account_list_get_size() > 0 )
        {
            gtk_widget_set_sensitive( GTK_WIDGET(callButton), TRUE );
            if (account_list_current_account_has_mailbox ())
                gtk_widget_set_sensitive( GTK_WIDGET(mailboxButton), TRUE );
        }
        else
        {
            gtk_widget_set_sensitive( GTK_WIDGET(callButton), FALSE);
        }
    }


}
