/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010 Savoir-Faire Linux Inc.
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

#include <toolbar.h>
#include <contacts/addressbook.h>


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
    account_id = g_strdup (current->accountID);

    create_new_call (CALL, CALL_STATE_DIALING, "", account_id, _("Voicemail"), to, &mailbox_call);
    DEBUG("Call: TO : %s" , mailbox_call->_peer_number);
    calllist_add( current_calls , mailbox_call );
    calltree_add_call( current_calls, mailbox_call, NULL);
    update_actions();
    sflphone_place_call( mailbox_call );
    calltree_display(current_calls);
}

/**
 * Make a call
 */
    static void
call_button( GtkWidget *widget UNUSED, gpointer   data UNUSED)
{
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
            calltree_add_call(current_calls, new_call, NULL);
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



/*
GtkWidget *create_toolbar ()
{
    GtkWidget *ret;
    GtkWidget *image;

    ret = gtk_toolbar_new();
    toolbar = ret;

    gtk_toolbar_set_orientation(GTK_TOOLBAR(ret), GTK_ORIENTATION_HORIZONTAL);
    gtk_toolbar_set_style(GTK_TOOLBAR(ret), GTK_TOOLBAR_ICONS);

    image = gtk_image_new_from_file( ICONS_DIR "/dial.svg");
    callButton = gtk_tool_button_new (image, _("New call"));
#if GTK_CHECK_VERSION(2,12,0)
    gtk_widget_set_tooltip_text(GTK_WIDGET(callButton), _("New call"));
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
    unholdButton = gtk_tool_button_new (image, _("Hold off"));
#if GTK_CHECK_VERSION(2,12,0)
    gtk_widget_set_tooltip_text(GTK_WIDGET(unholdButton), _("Hold off"));
#endif
    gtk_widget_set_state( GTK_WIDGET(unholdButton), GTK_STATE_INSENSITIVE);
    g_signal_connect (G_OBJECT (unholdButton), "clicked",
            G_CALLBACK (unhold), NULL);
    //gtk_toolbar_insert(GTK_TOOLBAR(ret), GTK_TOOL_ITEM(unholdButton), -1);
    gtk_widget_show_all(GTK_WIDGET(unholdButton));

    image = gtk_image_new_from_file( ICONS_DIR "/hold.svg");
    holdButton =  gtk_tool_button_new (image, _("Hold on"));
#if GTK_CHECK_VERSION(2,12,0)
    gtk_widget_set_tooltip_text(GTK_WIDGET(holdButton), _("Hold on"));
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
    gtk_widget_set_tooltip_text(GTK_WIDGET(recButton), _("Record"));
#endif
    gtk_widget_set_state( GTK_WIDGET(recButton), GTK_STATE_INSENSITIVE);
    g_signal_connect (G_OBJECT (recButton), "clicked",
            G_CALLBACK (rec_button), NULL);
    gtk_toolbar_insert(GTK_TOOLBAR(ret), GTK_TOOL_ITEM(recButton), -1);

    return ret;
}
*/
