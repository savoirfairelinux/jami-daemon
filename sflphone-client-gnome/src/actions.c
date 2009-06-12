/*
 *  Copyright (C) 2007 - 2008 Savoir-Faire Linux inc.
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

#include <actions.h>
#include <dbus/dbus.h>
#include <statusicon.h>
#include <contacts/searchbar.h>
#include <menus.h>
#include <toolbar.h>

#include <gtk/gtk.h>
#include <string.h>
#include <glib/gprintf.h>
#include <stdlib.h>
#include <sys/types.h>
#include <unistd.h>

guint voice_mails;

    void
sflphone_notify_voice_mail ( const gchar* accountID , guint count )
{
    gchar *id;
    gchar *current;

    // We want to notify only for the default current account; ie the first in the list
    id = g_strdup( accountID );
    current = account_list_get_current_id();
    if( strcmp( id, current ) != 0 )
        return;

    voice_mails = count ;

    if(count > 0)
    {
        gchar * message = g_new0(gchar, 50);
        if( count > 1)
            g_sprintf(message, _("%d voice mails"), count);
        else
            g_sprintf(message, _("%d voice mail"), count);
        statusbar_push_message(message,  __MSG_VOICE_MAILS);
        g_free(message);
    }

    // TODO: add ifdef
    if( account_list_get_size() > 0 )
    {
        account_t* acc = account_list_get_by_id( id );
        if( acc != NULL )
            notify_voice_mails( count , acc );
    }
}

    void
status_bar_display_account ()
{
    gchar* msg;
    account_t* acc;

    statusbar_pop_message(__MSG_ACCOUNT_DEFAULT);

    acc = account_list_get_current ();
    if(acc){
        msg = g_markup_printf_escaped(_("Using %s (%s)") ,
                (gchar*)g_hash_table_lookup( acc->properties , ACCOUNT_ALIAS),
                (gchar*)g_hash_table_lookup( acc->properties , ACCOUNT_TYPE));
    }
    else
    {
        msg = g_markup_printf_escaped(_("No registered account"));
    }
    statusbar_push_message( msg , __MSG_ACCOUNT_DEFAULT);
    g_free(msg);
}


    gboolean
sflphone_quit ()
{
    gboolean quit = FALSE;
    guint count = calllist_get_size(current_calls);
    if(count > 0){
        quit = main_window_ask_quit();
    }
    else{
        quit = TRUE;
    }

    if (quit)
    {
        // Save the history 
        sflphone_save_history ();

        dbus_unregister(getpid());
        dbus_clean ();
        //call_list_clean(); TODO
        //account_list_clean()
        gtk_main_quit ();
    }
    return quit;
}

    void
sflphone_hold (callable_obj_t * c )
{
    c->_state = CALL_STATE_HOLD;
    calltree_update_call(current_calls,c);
    update_menus();
}

    void
sflphone_ringing(callable_obj_t * c )
{
    c->_state = CALL_STATE_RINGING;
    calltree_update_call(current_calls,c);
    update_menus();
}

    void
sflphone_hung_up( callable_obj_t * c)
{
    calllist_remove( current_calls, c->_callID);
    calltree_remove_call(current_calls, c);
    c->_state = CALL_STATE_DIALING;
    update_menus();
#if GTK_CHECK_VERSION(2,10,0)
    status_tray_icon_blink( FALSE );
#endif
}

/** Internal to actions: Fill account list */
    void
sflphone_fill_account_list(gboolean toolbarInitialized)
{

    gchar** array;
    gchar** accountID;
    unsigned int i;

    account_list_clear ( );

    array = (gchar **)dbus_account_list();
    if(array)
    {
        for (accountID = array; *accountID; accountID++)
        {
            account_t * a = g_new0(account_t,1);
            a->accountID = g_strdup(*accountID);
            account_list_add(a);
        }
        g_strfreev (array);
    }

    for( i = 0; i < account_list_get_size(); i++)
    {
        account_t  * a = account_list_get_nth (i);
        GHashTable * details = (GHashTable *) dbus_account_details(a->accountID);
        if( details == NULL )
            break;
        a->properties = details;

        gchar * status = g_hash_table_lookup(details, "Status");
        if(strcmp(status, "REGISTERED") == 0)
        {
            a->state = ACCOUNT_STATE_REGISTERED;
        }
        else if(strcmp(status, "UNREGISTERED") == 0)
        {
            a->state = ACCOUNT_STATE_UNREGISTERED;
        }
        else if(strcmp(status, "TRYING") == 0)
        {
            a->state = ACCOUNT_STATE_TRYING;
        }
        else if(strcmp(status, "ERROR") == 0)
        {
            a->state = ACCOUNT_STATE_ERROR;
        }
        else if(strcmp( status , "ERROR_AUTH") == 0 )
        {
            a->state = ACCOUNT_STATE_ERROR_AUTH;
        }
        else if(strcmp( status , "ERROR_NETWORK") == 0 )
        {
            a->state = ACCOUNT_STATE_ERROR_NETWORK;
        }
        else if(strcmp( status , "ERROR_HOST") == 0 )
        {
            a->state = ACCOUNT_STATE_ERROR_HOST;
        }
        else if(strcmp( status , "ERROR_CONF_STUN") == 0 )
        {
            a->state = ACCOUNT_STATE_ERROR_CONF_STUN;
        }
        else if(strcmp( status , "ERROR_EXIST_STUN") == 0 )
        {
            a->state = ACCOUNT_STATE_ERROR_EXIST_STUN;
        }
        else
        {
            a->state = ACCOUNT_STATE_INVALID;
        }

    }

    // Prevent update being called when toolbar is not yet initialized
    if(toolbarInitialized)
        toolbar_update_buttons();
}

gboolean sflphone_init()
{
    if(!dbus_connect ()){

        main_window_error_message(_("Unable to connect to the SFLphone server.\nMake sure the daemon is running."));
        return FALSE;
    }
    else
    {
        dbus_register(getpid(), "Gtk+ Client");

        current_calls = calltab_init(FALSE, CURRENT_CALLS);
        contacts = calltab_init(TRUE, CONTACTS);
        history = calltab_init(TRUE, HISTORY);

        account_list_init ();
        codec_list_init();

        // Fetch the configured accounts
        sflphone_fill_account_list(FALSE);

        // Fetch the audio codecs
        sflphone_fill_codec_list();

        return TRUE;
    }
}

    void
sflphone_hang_up()
{
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls);
    if(selectedCall)
    {
        switch(selectedCall->_state)
        {
            case CALL_STATE_DIALING:
                dbus_hang_up (selectedCall);
                break;
            case CALL_STATE_RINGING:
                dbus_hang_up (selectedCall);
                selectedCall->_state = CALL_STATE_DIALING;
                //selectedCall->_stop = 0;
                break;
            case CALL_STATE_CURRENT:
            case CALL_STATE_HOLD:
            case CALL_STATE_BUSY:
            case CALL_STATE_RECORD:
                dbus_hang_up (selectedCall);
                selectedCall->_state = CALL_STATE_DIALING;
                set_timestamp (&selectedCall->_time_stop);
                break;
            case CALL_STATE_FAILURE:
                dbus_hang_up (selectedCall);
                selectedCall->_state = CALL_STATE_DIALING;
                break;
            case CALL_STATE_INCOMING:
                dbus_refuse (selectedCall);
                selectedCall->_state = CALL_STATE_DIALING;
                DEBUG("from sflphone_hang_up : "); stop_notification();
                break;
            case CALL_STATE_TRANSFERT:
                dbus_hang_up (selectedCall);
                set_timestamp (&selectedCall->_time_stop);
                break;
            default:
                WARN("Should not happen in sflphone_hang_up()!");
                break;
        }
    }
    calltree_update_call( history , selectedCall );
}


    void
sflphone_pick_up()
{
    DEBUG("sflphone_pick_up\n");
    callable_obj_t * selectedCall = calltab_get_selected_call(active_calltree);
    if(selectedCall)
    {
        switch(selectedCall->_state)
        {
            case CALL_STATE_DIALING:
                sflphone_place_call (selectedCall);
                break;
            case CALL_STATE_INCOMING:
                selectedCall->_history_state = INCOMING;
                calltree_update_call( history , selectedCall );
                dbus_accept (selectedCall);
                DEBUG("from sflphone_pick_up : "); stop_notification();
                break;
            case CALL_STATE_HOLD:
                sflphone_new_call();
                break;
            case CALL_STATE_TRANSFERT:
                dbus_transfert (selectedCall);
                set_timestamp (&selectedCall->_time_stop);
                break;
            case CALL_STATE_CURRENT:
            case CALL_STATE_RECORD:
                sflphone_new_call();
                break;
            case CALL_STATE_RINGING:
                sflphone_new_call();
                break;
            default:
                WARN("Should not happen in sflphone_pick_up()!");
                break;
        }
    }
    else {
        sflphone_new_call();
    }

}

    void
sflphone_on_hold ()
{
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls);
    if(selectedCall)
    {
        switch(selectedCall->_state)
        {
            case CALL_STATE_CURRENT:
                dbus_hold (selectedCall);
                break;
            case CALL_STATE_RECORD:
                dbus_hold (selectedCall);
                break;

            default:
                WARN("Should not happen in sflphone_on_hold!");
                break;
        }
    }
}

    void
sflphone_off_hold ()
{
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls);
    if(selectedCall)
    {
        switch(selectedCall->_state)
        {
            case CALL_STATE_HOLD:
                dbus_unhold (selectedCall);
                break;
            default:
                WARN("Should not happen in sflphone_off_hold ()!");
                break;
        }
    }

    if(dbus_get_is_recording(selectedCall))
    {
        DEBUG("Currently recording!");
    }
    else
    {
        DEBUG("Not recording currently");
    }
}


    void
sflphone_fail( callable_obj_t * c )
{
    c->_state = CALL_STATE_FAILURE;
    calltree_update_call(current_calls,c);
    update_menus();
}

    void
sflphone_busy( callable_obj_t * c )
{
    c->_state = CALL_STATE_BUSY;
    calltree_update_call(current_calls, c);
    update_menus();
}

    void
sflphone_current( callable_obj_t * c )
{
    if( c->_state != CALL_STATE_HOLD )
        set_timestamp (&c->_time_start);
    c->_state = CALL_STATE_CURRENT;
    calltree_update_call(current_calls,c);
    update_menus();
}

    void
sflphone_record( callable_obj_t * c )
{
    if( c->_state != CALL_STATE_HOLD )
        set_timestamp (&c->_time_start);
    c->_state = CALL_STATE_RECORD;
    calltree_update_call(current_calls,c);
    update_menus();
}

    void
sflphone_set_transfert()
{
    callable_obj_t * c = calltab_get_selected_call(current_calls);
    if(c)
    {
        c->_state = CALL_STATE_TRANSFERT;
        c->_trsft_to = g_strdup("");
        calltree_update_call(current_calls,c);
        update_menus();
    }
    toolbar_update_buttons();
}

    void
sflphone_unset_transfert()
{
    callable_obj_t * c = calltab_get_selected_call(current_calls);
    if(c)
    {
        c->_state = CALL_STATE_CURRENT;
        c->_trsft_to = g_strdup("");
        calltree_update_call(current_calls,c);
        update_menus();
    }
    toolbar_update_buttons();
}

    void
sflphone_display_transfer_status(const gchar* message)
{
    statusbar_push_message( message , __MSG_ACCOUNT_DEFAULT);
}

    void
sflphone_incoming_call (callable_obj_t * c)
{
    c->_history_state = MISSED;
    calllist_add ( current_calls, c );
    calllist_add( history, c );
    calltree_add_call( current_calls , c );
    update_menus();
    calltree_display (current_calls);
}

    void
process_dialing(callable_obj_t * c, guint keyval, gchar * key)
{
    // We stop the tone
    if(strlen(c->_peer_number) == 0 && c->_state != CALL_STATE_TRANSFERT){
        dbus_start_tone( FALSE , 0 );
        //dbus_play_dtmf( key );
    }

    DEBUG("process_dialing : keyval : %i",keyval);
    DEBUG("process_dialing : key : %s",key);

    switch (keyval)
    {
        case 65293: /* ENTER */
        case 65421: /* ENTER numpad */
            sflphone_place_call(c);
            break;
        case 65307: /* ESCAPE */
            sflphone_hang_up(c);
            break;
        case 65288: /* BACKSPACE */
            {  /* Brackets mandatory because of local vars */
                gchar * before = c->_peer_number;
                if(strlen(c->_peer_number) >= 1){

                    c->_peer_number = g_strndup(c->_peer_number, strlen(c->_peer_number) -1);
                    g_free(before);
                    DEBUG("TO: backspace %s", c->_peer_number);

                    if(c->_state == CALL_STATE_DIALING)
                    {
                        //g_free(c->_peer_name);
                        //c->_peer_name = g_strconcat("\"\" <", c->_peer_number, ">", NULL);
                    }
                    calltree_update_call(current_calls,c);
                }
                else if(strlen(c->_peer_number) == 0)
                {
                    if(c->_state != CALL_STATE_TRANSFERT)
                        dbus_hang_up(c);
                }
            }
            break;
        case 65289: /* TAB */
        case 65513: /* ALT */
        case 65507: /* CTRL */
        case 65515: /* SUPER */
        case 65509: /* CAPS */
            break;
        default:
            // if (keyval < 255 || (keyval >65453 && keyval < 65466))
            if (keyval < 127 || (keyval > 65400 && keyval < 65466))
            {

                if (c->_state == CALL_STATE_TRANSFERT)
                {
                    c->_trsft_to = g_strconcat(c->_trsft_to, key, NULL);
                }
                else
                {
                    dbus_play_dtmf( key );
                    c->_peer_number = g_strconcat(c->_peer_number, key, NULL);
                }

                if(c->_state == CALL_STATE_DIALING)
                {
                    //g_free(c->_peer_name);
                    //c->_peer_name = g_strconcat("\"\" <", c->_peer_number, ">", NULL);
                }
                calltree_update_call(current_calls,c);
            }
            break;
    }
}


    callable_obj_t *
sflphone_new_call()
{

    callable_obj_t *c;
    gchar *peer_name, *peer_number;

    DEBUG("sflphone_new_call\n");
    sflphone_on_hold();

    // Play a tone when creating a new call
    if( calllist_get_size(current_calls) == 0 )
        dbus_start_tone( TRUE , ( voice_mails > 0 )? TONE_WITH_MESSAGE : TONE_WITHOUT_MESSAGE) ;

    peer_number = g_strdup("");
    peer_name = g_strdup ("");
    create_new_call (CALL, CALL_STATE_DIALING, "", "", peer_name, peer_number, &c);

    calllist_add (current_calls,c);
    calltree_add_call (current_calls,c);
    update_menus();

    return c;
}


    void
sflphone_keypad( guint keyval, gchar * key)
{
    DEBUG("sflphone_keypad \n");
    callable_obj_t * c = calltab_get_selected_call(current_calls);

    if((active_calltree != current_calls) || (active_calltree == current_calls && !c))
    {
        // Not in a call, not dialing, create a new call
        //dbus_play_dtmf(key);
        switch (keyval)
        {
            case 65293: /* ENTER */
            case 65421: /* ENTER numpad */
            case 65307: /* ESCAPE */
                break;
            default:
                calltree_display (current_calls);
                process_dialing(sflphone_new_call(), keyval, key);
                break;
        }
    }
    else if(c)
    {
        DEBUG("call");
        switch(c->_state)
        {
            case CALL_STATE_DIALING: // Currently dialing => edit number
                DEBUG("Writing a number\n");
                process_dialing(c, keyval, key);
                break;
            case CALL_STATE_RECORD:
            case CALL_STATE_CURRENT:
                switch (keyval)
                {
                    case 65307: /* ESCAPE */
                        dbus_hang_up(c);
                        set_timestamp (&c->_time_stop);
                        calltree_update_call( history , c );
                        break;
                    default:
                        // To play the dtmf when calling mail box for instance
                        dbus_play_dtmf(key);
                        if (keyval < 255 || (keyval >65453 && keyval < 65466))
                        {
                            //gchar * temp = g_strconcat(call_get_number(c), key, NULL);
                            //gchar * before = c->from;
                            //c->from = g_strconcat("\"",call_get_name(c) ,"\" <", temp, ">", NULL);
                            //g_free(before);
                            //g_free(temp);
                            //update_callable_obj_tree(current_calls,c);
                        }
                        break;
                }
                break;
            case CALL_STATE_INCOMING:
                switch (keyval)
                {
                    case 65293: /* ENTER */
                    case 65421: /* ENTER numpad */
                        c->_history_state = INCOMING;
                        calltree_update_call( history , c );
                        dbus_accept(c);
                        DEBUG("from sflphone_keypad ( enter ) : "); stop_notification();
                        break;
                    case 65307: /* ESCAPE */
                        dbus_refuse(c);
                        DEBUG("from sflphone_keypad ( escape ) : "); stop_notification();
                        break;
                }
                break;
            case CALL_STATE_TRANSFERT:
                switch (keyval)
                {
                    case 65293: /* ENTER */
                    case 65421: /* ENTER numpad */
                        dbus_transfert(c);
                        set_timestamp (&c->_time_stop);
                        break;
                    case 65307: /* ESCAPE */
                        sflphone_unset_transfert(c);
                        break;
                    default: // When a call is on transfert, typing new numbers will add it to c->_peer_number
                        process_dialing(c, keyval, key);
                        break;
                }
                break;
            case CALL_STATE_HOLD:
                switch (keyval)
                {
                    case 65293: /* ENTER */
                    case 65421: /* ENTER numpad */
                        dbus_unhold(c);
                        break;
                    case 65307: /* ESCAPE */
                        dbus_hang_up(c);
                        break;
                    default: // When a call is on hold, typing new numbers will create a new call
                        process_dialing(sflphone_new_call(), keyval, key);
                        break;
                }
                break;
            case CALL_STATE_RINGING:
            case CALL_STATE_BUSY:
            case CALL_STATE_FAILURE:
                //c->_stop = 0;
                switch (keyval)
                {
                    case 65307: /* ESCAPE */
                        dbus_hang_up(c);
                        //c->_stop = 0;
                        calltree_update_call( history , c );
                        break;
                }
                break;
            default:
                break;
        }

    }
    else {
        sflphone_new_call();
    }
}

/*
 * Place a call with the current account.
 * If there is no default account selected, place a call with the first
 * registered account of the account list
 * Else, check if it an IP call. if not, popup an error message
 */
    void
sflphone_place_call ( callable_obj_t * c )
{

    if (c)
    {

        if (c->_state == CALL_STATE_DIALING && g_str_has_prefix (c->_peer_number, "sip:"))
        {
            dbus_place_call (c);
        }

        else {

            if(c->_state == CALL_STATE_DIALING && strcmp(c->_peer_number, "") != 0)
            {

                if( account_list_get_size() == 0 )
                {
                    notify_no_accounts();
                    sflphone_fail(c);
                }

                else if( account_list_get_by_state( ACCOUNT_STATE_REGISTERED ) == NULL )
                {
                    notify_no_registered_accounts();
                    sflphone_fail(c);
                }

                else
                {

                    account_t * current;

                    if(g_strcasecmp(c->_accountID, "") != 0) {
                        current = account_list_get_by_id(c->_accountID);
                    } else {
                        current = account_list_get_current();
                    }
                    // DEBUG("sflphone_place_call :: c->accountID : %i",c->accountID);

                    // account_t * current = c->accountID;


                    if( current )
                    {

                        if(g_strcasecmp(g_hash_table_lookup( current->properties, "Status"),"REGISTERED")==0)
                        {
                            // OK, everything alright - the call is made with the current account
                            c->_accountID = current -> accountID;
                            dbus_place_call(c);
                        }
                        else
                        {
                            // Current account is not registered
                            // So we place a call with the first registered account
                            // And we switch the current account
                            current = account_list_get_by_state( ACCOUNT_STATE_REGISTERED );
                            c->_accountID = current -> accountID;
                            dbus_place_call(c);
                            notify_current_account( current );
                        }
                    }
                    else
                    {

                        // No current accounts have been setup.
                        // So we place a call with the first registered account
                        // and we change the current account
                        current = account_list_get_by_state( ACCOUNT_STATE_REGISTERED );
                        c->_accountID = current -> accountID;
                        dbus_place_call(c);
                        notify_current_account( current );
                    }
                }
                // Update history
                c->_history_state = OUTGOING;
                calllist_add(history, c);
            }
        }
    }
}

    void
sflphone_display_selected_codec (const gchar* codecName)
{

    callable_obj_t * selectedCall;
    gchar* msg;
    account_t* acc;

    selectedCall =  calltab_get_selected_call(current_calls);
    if (selectedCall) {
        if(selectedCall->_accountID != NULL){
            acc = account_list_get_by_id(selectedCall->_accountID);
            if (!acc) {
                msg = g_markup_printf_escaped (_("IP call - %s"), codecName);
            }
            else {
                msg = g_markup_printf_escaped(_("Using %s (%s) - Codec: %s") ,
                        (gchar*)g_hash_table_lookup( acc->properties , ACCOUNT_ALIAS),
                        (gchar*)g_hash_table_lookup( acc->properties , ACCOUNT_TYPE),
                        codecName);
            }
            statusbar_push_message( msg , __MSG_ACCOUNT_DEFAULT);
            g_free(msg);
        }
    }
}

    gchar*
sflphone_get_current_codec_name()
{
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls);
    return dbus_get_current_codec_name(selectedCall);
}

    void
sflphone_rec_call()
{
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls);
    dbus_set_record(selectedCall);


    switch(selectedCall->_state)
    {
        case CALL_STATE_CURRENT:
            selectedCall->_state = CALL_STATE_RECORD;
            break;
        case CALL_STATE_RECORD:
            selectedCall->_state = CALL_STATE_CURRENT;
            break;
        default:
            WARN("Should not happen in sflphone_off_hold ()!");
            break;
    }
    calltree_update_call(current_calls,selectedCall);
    update_menus();

    // gchar* codname = sflphone_get_current_codec_name();
    // DEBUG("sflphone_get_current_codec_name: %s",codname);
}

/* Internal to action - get the codec list */
    void
sflphone_fill_codec_list()
{
    codec_list_clear();

    gchar** codecs = (gchar**)dbus_codec_list();
    gchar** order = (gchar**)dbus_get_active_codec_list();
    gchar** details;
    gchar** pl;

    for(pl=order; *order; order++)
    {
        codec_t * c = g_new0(codec_t, 1);
        c->_payload = atoi(*order);
        details = (gchar **)dbus_codec_details(c->_payload);

        //DEBUG("Codec details: %s / %s / %s / %s",details[0],details[1],details[2],details[3]);

        c->name = details[0];
        c->is_active = TRUE;
        c->sample_rate = atoi(details[1]);
        c->_bitrate = atof(details[2]);
        c->_bandwidth = atof(details[3]);
        codec_list_add(c);
    }

    for(pl=codecs; *codecs; codecs++)
    {
        details = (gchar **)dbus_codec_details(atoi(*codecs));
        if(codec_list_get_by_payload(atoi(*codecs))!=NULL){
            // does nothing - the codec is already in the list, so is active.
        }
        else{
            codec_t* c = g_new0(codec_t, 1);
            c->_payload = atoi(*codecs);
            c->name = details[0];
            c->is_active = FALSE;
            c->sample_rate = atoi(details[1]);
            c->_bitrate = atof(details[2]);
            c->_bandwidth = atof(details[3]);
            codec_list_add(c);
        }
    }
    if( codec_list_get_size() == 0) {

        gchar* markup = g_markup_printf_escaped(_("<b>Error: No audio codecs found.\n\n</b> SFL audio codecs have to be placed in <i>%s</i> or in the <b>.sflphone</b> directory in your home( <i>%s</i> )") , CODECS_DIR , g_get_home_dir());
        main_window_error_message( markup );
        dbus_unregister(getpid());
        exit(0);
    }
}

void sflphone_fill_call_list (void)
{

    gchar** calls = (gchar**)dbus_get_call_list();
    gchar** pl;
    GHashTable *call_details;
    callable_obj_t *c;
    gchar *callID;

    if(calls)
    {
        for(pl=calls; *calls; calls++)
        {
            c = g_new0(callable_obj_t, 1);
            callID = (gchar*)(*calls);
            call_details = dbus_get_call_details(callID);
            create_new_call_from_details (callID, call_details, &c);
            c->_callID = g_strdup(callID);

            // Add it to the list
            DEBUG ("Add call retrieved from server side: %s\n", c->_callID);
            calllist_add (current_calls, c);
            // Update the GUI
            calltree_add_call (current_calls, c);
        }
    }
}

void sflphone_fill_history (void)
{
    GHashTable *entries;
    GHashTableIter iter;
    gpointer key, value;
    callable_obj_t *history_entry;

    DEBUG ("Loading history ...");

    entries = dbus_get_history ();
    if (entries)
    {
        // Init the iterator
        g_hash_table_iter_init (&iter, entries);
        while (g_hash_table_iter_next (&iter, &key, &value)) 
        {
            /* do something with key and value */
            create_history_entry_from_serialized_form ((gchar*)key, (gchar*)value, &history_entry);    
            // Add it and update the GUI
            calllist_add (history, history_entry);
        }
    }
}

void sflphone_save_history (void)
{
    GQueue *items;
    gint size;
    int i;
    callable_obj_t *current;
    GHashTable *result = NULL;
    gchar *key, *value;

    DEBUG ("Saving history ...");

    result = g_hash_table_new(NULL, g_str_equal);
    items = history->callQueue;
    size = calllist_get_size (history);

    for (i=0; i<size; i++)
    {
        current = g_queue_peek_nth (items, i);
        if (current)
        {
            value = serialize_history_entry (current);
            key = convert_timestamp_to_gchar (current->_time_start);
            g_hash_table_replace(result, (gpointer) key,
                    (gpointer) value);
        }
    }

    dbus_set_history (result);

    // Decrement the reference count
    g_hash_table_unref(result);
}

