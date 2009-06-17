/*
 *  Copyright (C) 2007 Savoir-Faire Linux inc.
 *  Author: Pierre-Luc Beaudoin <pierre-luc.beaudoin@savoirfairelinux.com>
 *  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
 *  Author: Guillaume Carmel-Archambault <guillaume.carmel-archambault@savoirfairelinux.com>
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

#include <calltab.h>
#include <callmanager-glue.h>
#include <configurationmanager-glue.h>
#include <instance-glue.h>
#include <configwindow.h>
#include <mainwindow.h>
#include <marshaller.h>
#include <sliders.h>
#include <statusicon.h>
#include <assistant.h>

#include <dbus.h>
#include <actions.h>
#include <string.h>

DBusGConnection * connection;
DBusGProxy * callManagerProxy;
DBusGProxy * configurationManagerProxy;
DBusGProxy * instanceProxy;

    static void
incoming_call_cb (DBusGProxy *proxy UNUSED,
        const gchar* accountID,
        const gchar* callID,
        const gchar* from,
        void * foo  UNUSED )
{
    DEBUG ("Incoming call! %s",callID);

    callable_obj_t * c;
    gchar *peer_name, *peer_number;
    // We receive the from field under a formatted way. We want to extract the number and the name of the caller
    peer_name = call_get_peer_name (from);
    peer_number = call_get_peer_number (from);

    create_new_call (CALL, CALL_STATE_INCOMING, g_strdup(callID), g_strdup(accountID), peer_name, peer_number, &c);
#if GTK_CHECK_VERSION(2,10,0)
    status_tray_icon_blink( TRUE );
#endif

    set_timestamp (&c->_time_start);
    notify_incoming_call (c);
    sflphone_incoming_call (c);
}

    static void
curent_selected_codec (DBusGProxy *proxy UNUSED,
        const gchar* callID,
        const gchar* codecName,
        void * foo  UNUSED )
{
    DEBUG ("%s codec decided for call %s",codecName,callID);
    sflphone_display_selected_codec (codecName);
}

    static void
volume_changed_cb (DBusGProxy *proxy UNUSED,
        const gchar* device,
        const gdouble value,
        void * foo  UNUSED )
{
    DEBUG ("Volume of %s changed to %f.",device, value);
    set_slider(device, value);
}

    static void
voice_mail_cb (DBusGProxy *proxy UNUSED,
        const gchar* accountID,
        const guint nb,
        void * foo  UNUSED )
{
    DEBUG ("%d Voice mail waiting!",nb);
    sflphone_notify_voice_mail (accountID , nb);
}

    static void
incoming_message_cb (DBusGProxy *proxy UNUSED,
        const gchar* accountID UNUSED,
        const gchar* msg,
        void * foo  UNUSED )
{
    DEBUG ("Message %s!",msg);

}

    static void
call_state_cb (DBusGProxy *proxy UNUSED,
        const gchar* callID,
        const gchar* state,
        void * foo  UNUSED )
{
    DEBUG ("Call %s state %s",callID, state);
    callable_obj_t * c = calllist_get(current_calls, callID);
    if(c)
    {
        if ( strcmp(state, "HUNGUP") == 0 )
        {
            if(c->_state==CALL_STATE_CURRENT)
            {
                // peer hung up, the conversation was established, so _start has been initialized with the current time value
                DEBUG("call state current");
                set_timestamp (&c->_time_start);
                calltree_update_call( history, c );
            }
            stop_notification();
            sflphone_hung_up (c);
            calltree_update_call( history, c );
        }
        else if ( strcmp(state, "UNHOLD_CURRENT") == 0 )
        {
            sflphone_current (c);
        }
        else if ( strcmp(state, "UNHOLD_RECORD") == 0 )
        {
            sflphone_record (c);
        }
        else if ( strcmp(state, "HOLD") == 0 )
        {
            sflphone_hold (c);
        }
        else if ( strcmp(state, "RINGING") == 0 )
        {
            sflphone_ringing (c);
        }
        else if ( strcmp(state, "CURRENT") == 0 )
        {
            sflphone_current (c);
        }
        else if ( strcmp(state, "FAILURE") == 0 )
        {
            sflphone_fail (c);
        }
        else if ( strcmp(state, "BUSY") == 0 )
        {
            sflphone_busy (c);
        }
    }
    else
    {
        // The callID is unknow, threat it like a new call
        // If it were an incoming call, we won't be here
        // It means that a new call has been initiated with an other client (cli for instance)
        if ( strcmp(state, "RINGING") == 0 )
        {
            callable_obj_t *new_call;
            GHashTable *call_details;
            gchar *type;

            DEBUG ("New ringing call! accountID: %s", callID);

            // We fetch the details associated to the specified call
            call_details = dbus_get_call_details (callID);
            create_new_call_from_details (callID, call_details, &new_call);

            // Restore the callID to be synchronous with the daemon
            new_call->_callID = g_strdup(callID);
            type = g_hash_table_lookup (call_details, "CALL_TYPE");

            if (g_strcasecmp (type, "0") == 0)
            {
                g_print ("incoming\n");
                new_call->_history_state = INCOMING;
            }
            else
            {
                g_print ("outgoing\n");
                new_call->_history_state = OUTGOING;
            }

            calllist_add (current_calls, new_call);
            calllist_add (history, new_call);
            calltree_add_call (current_calls, new_call);
            update_menus ();
            calltree_display (current_calls);

            //sflphone_incoming_call (new_call);
        }
    }
}


    static void
accounts_changed_cb (DBusGProxy *proxy UNUSED,
        void * foo  UNUSED )
{
    DEBUG ("Accounts changed");
    sflphone_fill_account_list(TRUE);
    config_window_fill_account_list();

    // Update the status bar in case something happened
    // Should fix ticket #1215
    status_bar_display_account();
}


    static void
transfer_succeded_cb (DBusGProxy *proxy UNUSED,
        void * foo  UNUSED )
{
    DEBUG ("Transfer succeded\n");
    sflphone_display_transfer_status("Transfer successfull");
}


    static void
transfer_failed_cb (DBusGProxy *proxy UNUSED,
        void * foo  UNUSED )
{
    DEBUG ("Transfer failed\n");
    sflphone_display_transfer_status("Transfer failed");
}


    static void
error_alert(DBusGProxy *proxy UNUSED,
        int errCode,
        void * foo  UNUSED )
{
    ERROR ("Error notifying : (%i)", errCode);
    sflphone_throw_exception( errCode );
}

    gboolean
dbus_connect ()
{

    GError *error = NULL;
    connection = NULL;
    instanceProxy = NULL;

    g_type_init ();

    connection = dbus_g_bus_get (DBUS_BUS_SESSION, &error);

    if (error)
    {
        ERROR ("Failed to open connection to bus: %s",
                error->message);
        g_error_free (error);
        return FALSE;
    }

    /* Create a proxy object for the "bus driver" (name "org.freedesktop.DBus") */


    instanceProxy = dbus_g_proxy_new_for_name (connection,
            "org.sflphone.SFLphone",
            "/org/sflphone/SFLphone/Instance",
            "org.sflphone.SFLphone.Instance");
    /*
       instanceProxy = dbus_g_proxy_new_for_name_owner (connection,
       "org.sflphone.SFLphone",
       "/org/sflphone/SFLphone/Instance",
       "org.sflphone.SFLphone.Instance",
       &error);
     */

    if (instanceProxy==NULL)
    {
        ERROR ("Failed to get proxy to Instance");
        return FALSE;
    }


    DEBUG ("DBus connected to Instance");


    callManagerProxy = dbus_g_proxy_new_for_name (connection,
            "org.sflphone.SFLphone",
            "/org/sflphone/SFLphone/CallManager",
            "org.sflphone.SFLphone.CallManager");

    /*
       callManagerProxy = dbus_g_proxy_new_for_name_owner (connection,
       "org.sflphone.SFLphone",
       "/org/sflphone/SFLphone/CallManager",
       "org.sflphone.SFLphone.CallManager",
       &error);
     */
    if (callManagerProxy==NULL)
    {
        ERROR ("Failed to get proxy to CallManagers");
        return FALSE;
    }

    DEBUG ("DBus connected to CallManager");
    /* Incoming call */
    dbus_g_object_register_marshaller(g_cclosure_user_marshal_VOID__STRING_STRING_STRING,
            G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (callManagerProxy,
            "incomingCall", G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy,
            "incomingCall", G_CALLBACK(incoming_call_cb), NULL, NULL);

    /* Current codec */
    dbus_g_object_register_marshaller(g_cclosure_user_marshal_VOID__STRING_STRING_STRING,
            G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (callManagerProxy,
            "currentSelectedCodec", G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy,
            "currentSelectedCodec", G_CALLBACK(curent_selected_codec), NULL, NULL);

    /* Register a marshaller for STRING,STRING */
    dbus_g_object_register_marshaller(g_cclosure_user_marshal_VOID__STRING_STRING,
            G_TYPE_NONE, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (callManagerProxy,
            "callStateChanged", G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy,
            "callStateChanged", G_CALLBACK(call_state_cb), NULL, NULL);

    dbus_g_object_register_marshaller(g_cclosure_user_marshal_VOID__STRING_INT,
            G_TYPE_NONE, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (callManagerProxy,
            "voiceMailNotify", G_TYPE_STRING, G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy,
            "voiceMailNotify", G_CALLBACK(voice_mail_cb), NULL, NULL);

    dbus_g_proxy_add_signal (callManagerProxy,
            "incomingMessage", G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy,
            "incomingMessage", G_CALLBACK(incoming_message_cb), NULL, NULL);

    dbus_g_object_register_marshaller(g_cclosure_user_marshal_VOID__STRING_DOUBLE,
            G_TYPE_NONE, G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (callManagerProxy,
            "volumeChanged", G_TYPE_STRING, G_TYPE_DOUBLE, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy,
            "volumeChanged", G_CALLBACK(volume_changed_cb), NULL, NULL);

    dbus_g_proxy_add_signal (callManagerProxy,
            "transferSucceded", G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy,
            "transferSucceded", G_CALLBACK(transfer_succeded_cb), NULL, NULL);

    dbus_g_proxy_add_signal (callManagerProxy,
            "transferFailed", G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy,
            "transferFailed", G_CALLBACK(transfer_failed_cb), NULL, NULL);


    configurationManagerProxy = dbus_g_proxy_new_for_name (connection, 
            "org.sflphone.SFLphone",
            "/org/sflphone/SFLphone/ConfigurationManager",
            "org.sflphone.SFLphone.ConfigurationManager");


    /*
       configurationManagerProxy = dbus_g_proxy_new_for_name_owner (connection,
       "org.sflphone.SFLphone",
       "/org/sflphone/SFLphone/ConfigurationManager",
       "org.sflphone.SFLphone.ConfigurationManager",
       &error);
     */
    if (!configurationManagerProxy)
    {
        ERROR ("Failed to get proxy to ConfigurationManager");
        return FALSE;
    }
    DEBUG ("DBus connected to ConfigurationManager");
    dbus_g_proxy_add_signal (configurationManagerProxy,
            "accountsChanged", G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (configurationManagerProxy,
            "accountsChanged", G_CALLBACK(accounts_changed_cb), NULL, NULL);

    dbus_g_object_register_marshaller(g_cclosure_user_marshal_VOID__INT,
            G_TYPE_NONE, G_TYPE_INT , G_TYPE_INVALID);
    dbus_g_proxy_add_signal (configurationManagerProxy,
            "errorAlert", G_TYPE_INT , G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (configurationManagerProxy,
            "errorAlert", G_CALLBACK(error_alert), NULL, NULL);
    return TRUE;
}

    void
dbus_clean ()
{
    g_object_unref (callManagerProxy);
    g_object_unref (configurationManagerProxy);
    g_object_unref (instanceProxy);
}


    void
dbus_hold (const callable_obj_t * c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_hold ( callManagerProxy, c->_callID, &error);
    if (error)
    {
        ERROR ("Failed to call hold() on CallManager: %s",
                error->message);
        g_error_free (error);
    }
}

    void
dbus_unhold (const callable_obj_t * c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_unhold ( callManagerProxy, c->_callID, &error);
    if (error)
    {
        ERROR ("Failed to call unhold() on CallManager: %s",
                error->message);
        g_error_free (error);
    }
}

    void
dbus_hang_up (const callable_obj_t * c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_hang_up ( callManagerProxy, c->_callID, &error);
    if (error)
    {
        ERROR ("Failed to call hang_up() on CallManager: %s",
                error->message);
        g_error_free (error);
    }
}

    void
dbus_transfert (const callable_obj_t * c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_transfert ( callManagerProxy, c->_callID, c->_trsft_to, &error);
    if (error)
    {
        ERROR ("Failed to call transfert() on CallManager: %s",
                error->message);
        g_error_free (error);
    }
}

    void
dbus_accept (const callable_obj_t * c)
{
#if GTK_CHECK_VERSION(2,10,0)
    status_tray_icon_blink( FALSE );
#endif
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_accept ( callManagerProxy, c->_callID, &error);
    if (error)
    {
        ERROR ("Failed to call accept(%s) on CallManager: %s", c->_callID,
                (error->message == NULL ? g_quark_to_string(error->domain): error->message));
        g_error_free (error);
    }
}

    void
dbus_refuse (const callable_obj_t * c)
{
#if GTK_CHECK_VERSION(2,10,0)
    status_tray_icon_blink( FALSE );
#endif
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_refuse ( callManagerProxy, c->_callID, &error);
    if (error)
    {
        ERROR ("Failed to call refuse() on CallManager: %s",
                error->message);
        g_error_free (error);
    }
}


    void
dbus_place_call (const callable_obj_t * c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_place_call ( callManagerProxy, c->_accountID, c->_callID, c->_peer_number, &error);
    if (error)
    {
        ERROR ("Failed to call placeCall() on CallManager: %s",
                error->message);
        g_error_free (error);
    }
}

gchar**  dbus_account_list()
{
    GError *error = NULL;
    char ** array;

    if(!org_sflphone_SFLphone_ConfigurationManager_get_account_list ( configurationManagerProxy, &array, &error))
    {
        if(error->domain == DBUS_GERROR && error->code == DBUS_GERROR_REMOTE_EXCEPTION)
        {
            ERROR ("Caught remote method (get_account_list) exception  %s: %s", dbus_g_error_get_name(error), error->message);
        }
        else
        {
            ERROR("Error while calling get_account_list: %s", error->message);
        }
        g_error_free (error);
        return NULL;
    }
    else{
        DEBUG ("DBus called get_account_list() on ConfigurationManager");
        return array;
    }
}


GHashTable* dbus_account_details(gchar * accountID)
{
    GError *error = NULL;
    GHashTable * details;

    if(!org_sflphone_SFLphone_ConfigurationManager_get_account_details( configurationManagerProxy, accountID, &details, &error))
    {
        if(error->domain == DBUS_GERROR && error->code == DBUS_GERROR_REMOTE_EXCEPTION)
        {
            ERROR ("Caught remote method (get_account_details) exception  %s: %s", dbus_g_error_get_name(error), error->message);
        }
        else
        {
            ERROR("Error while calling get_account_details: %s", error->message);
        }
        g_error_free (error);
        return NULL;
    }
    else{
        return details;
    }
}

    void
dbus_send_register ( gchar* accountID , const guint expire)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_send_register ( configurationManagerProxy, accountID, expire ,&error);
    if (error)
    {
        ERROR ("Failed to call send_register() on ConfigurationManager: %s",
                error->message);
        g_error_free (error);
    }
}

    void
dbus_remove_account(gchar * accountID)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_remove_account (
            configurationManagerProxy,
            accountID,
            &error);
    if (error)
    {
        ERROR ("Failed to call remove_account() on ConfigurationManager: %s",
                error->message);
        g_error_free (error);
    }
}

    void
dbus_set_account_details(account_t *a)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_account_details (
            configurationManagerProxy,
            a->accountID,
            a->properties,
            &error);
    if (error)
    {
        ERROR ("Failed to call set_account_details() on ConfigurationManager: %s",
                error->message);
        g_error_free (error);
    }
}

    gchar*
dbus_add_account(account_t *a)
{
    gchar* accountId;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_add_account (
            configurationManagerProxy,
            a->properties,
            &accountId,
            &error);
    if (error)
    {
        ERROR ("Failed to call add_account() on ConfigurationManager: %s",
                error->message);
        g_error_free (error);
    }
    return accountId;
}

    void
dbus_set_volume(const gchar * device, gdouble value)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_set_volume(
            callManagerProxy,
            device,
            value,
            &error);

    if (error)
    {
        ERROR ("Failed to call set_volume() on callManagerProxy: %s",
                error->message);
        g_error_free (error);
    }
}


    gdouble
dbus_get_volume(const gchar * device)
{
    gdouble  value;
    GError *error = NULL;

    org_sflphone_SFLphone_CallManager_get_volume(
            callManagerProxy,
            device,
            &value,
            &error);

    if (error)
    {
        ERROR ("Failed to call get_volume() on callManagerProxy: %s",
                error->message);
        g_error_free (error);
    }
    return value;
}


    void
dbus_play_dtmf(const gchar * key)
{
    GError *error = NULL;

    org_sflphone_SFLphone_CallManager_play_dt_mf(
            callManagerProxy,
            key,
            &error);

    if (error)
    {
        ERROR ("Failed to call playDTMF() on callManagerProxy: %s",
                error->message);
        g_error_free (error);
    }
}

    void
dbus_start_tone(const int start , const guint type )
{
    GError *error = NULL;

    org_sflphone_SFLphone_CallManager_start_tone(
            callManagerProxy,
            start,
            type,
            &error);

    if (error)
    {
        ERROR ("Failed to call startTone() on callManagerProxy: %s",
                error->message);
        g_error_free (error);
    }
}

    void
dbus_register(int pid, gchar * name)
{
    GError *error = NULL;

    org_sflphone_SFLphone_Instance_register(
            instanceProxy,
            pid,
            name,
            &error);

    if (error)
    {
        ERROR ("Failed to call register() on instanceProxy: %s",
                error->message);
        g_error_free (error);
    }
}

    void
dbus_unregister(int pid)
{
    GError *error = NULL;

    org_sflphone_SFLphone_Instance_unregister(
            instanceProxy,
            pid,
            &error);

    if (error)
    {
        ERROR ("Failed to call unregister() on instanceProxy: %s",
                error->message);
        g_error_free (error);
    }
}

    gchar**
dbus_codec_list()
{

    GError *error = NULL;
    gchar** array;
    org_sflphone_SFLphone_ConfigurationManager_get_codec_list (
            configurationManagerProxy,
            &array,
            &error);

    if (error)
    {
        ERROR ("Failed to call get_codec_list() on ConfigurationManager: %s",
                error->message);
        g_error_free (error);
    }
    return array;
}

    gchar**
dbus_codec_details( int payload )
{

    GError *error = NULL;
    gchar ** array;
    org_sflphone_SFLphone_ConfigurationManager_get_codec_details (
            configurationManagerProxy,
            payload,
            &array,
            &error);

    if (error)
    {
        ERROR ("Failed to call get_codec_details() on ConfigurationManager: %s",
                error->message);
        g_error_free (error);
    }
    return array;
}

    gchar*
dbus_get_current_codec_name(const callable_obj_t * c)
{

    DEBUG("dbus_get_current_codec_name : CallID : %s", c->_callID);

    gchar* codecName;
    GError* error = NULL;

    org_sflphone_SFLphone_CallManager_get_current_codec_name (
            callManagerProxy,
            c->_callID,
            &codecName,
            &error);
    if(error)
    {
        g_error_free(error);
    }

    DEBUG("dbus_get_current_codec_name : codecName : %s", codecName);

    return codecName;
}



    gchar**
dbus_get_active_codec_list()
{

    gchar ** array;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_active_codec_list (
            configurationManagerProxy,
            &array,
            &error);

    if (error)
    {
        ERROR ("Failed to call get_active_codec_list() on ConfigurationManager: %s",
                error->message);
        g_error_free (error);
    }
    return array;
}

    void
dbus_set_active_codec_list(const gchar** list)
{

    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_active_codec_list (
            configurationManagerProxy,
            list,
            &error);

    if (error)
    {
        ERROR ("Failed to call set_active_codec_list() on ConfigurationManager: %s",
                error->message);
        g_error_free (error);
    }
}

/**
 * Get a list of input supported audio plugins
 */
    gchar**
dbus_get_input_audio_plugin_list()
{
    gchar** array;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_input_audio_plugin_list(
            configurationManagerProxy,
            &array,
            &error);
    if(error)
    {
        ERROR("Failed to call get_input_audio_plugin_list() on ConfigurationManager: %s", error->message);
        g_error_free(error);
    }
    return array;
}

/**
 * Get a list of output supported audio plugins
 */
    gchar**
dbus_get_output_audio_plugin_list()
{
    gchar** array;
    GError* error = NULL;

    if(!org_sflphone_SFLphone_ConfigurationManager_get_output_audio_plugin_list( configurationManagerProxy, &array, &error))
    {
        if(error->domain == DBUS_GERROR && error->code == DBUS_GERROR_REMOTE_EXCEPTION)
        {
            ERROR ("Caught remote method (get_output_audio_plugin_list) exception  %s: %s", dbus_g_error_get_name(error), error->message);
        }
        else
        {
            ERROR("Error while calling get_out_audio_plugin_list: %s", error->message);
        }
        g_error_free (error);
        return NULL;
    }
    else{
        return array;
    }
}

    void
dbus_set_input_audio_plugin(gchar* audioPlugin)
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_input_audio_plugin(
            configurationManagerProxy,
            audioPlugin,
            &error);
    if(error)
    {
        ERROR("Failed to call set_input_audio_plugin() on ConfigurationManager: %s", error->message);
        g_error_free(error);
    }
}

    void
dbus_set_output_audio_plugin(gchar* audioPlugin)
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_output_audio_plugin(
            configurationManagerProxy,
            audioPlugin,
            &error);
    if(error)
    {
        ERROR("Failed to call set_output_audio_plugin() on ConfigurationManager: %s", error->message);
        g_error_free(error);
    }
}

/**
 * Get all output devices index supported by current audio manager
 */
gchar** dbus_get_audio_output_device_list()
{
    gchar** array;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_audio_output_device_list(
            configurationManagerProxy,
            &array,
            &error);
    if(error)
    {
        ERROR("Failed to call get_audio_output_device_list() on ConfigurationManager: %s", error->message);
        g_error_free(error);
    }
    return array;
}

/**
 * Set audio output device from its index
 */
    void
dbus_set_audio_output_device(const int index)
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_audio_output_device(
            configurationManagerProxy,
            index,
            &error);
    if(error)
    {
        ERROR("Failed to call set_audio_output_device() on ConfigurationManager: %s", error->message);
        g_error_free(error);
    }
}

/**
 * Get all input devices index supported by current audio manager
 */
    gchar**
dbus_get_audio_input_device_list()
{
    gchar** array;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_audio_input_device_list(
            configurationManagerProxy,
            &array,
            &error);
    if(error)
    {
        ERROR("Failed to call get_audio_input_device_list() on ConfigurationManager: %s", error->message);
        g_error_free(error);
    }
    return array;
}

/**
 * Set audio input device from its index
 */
    void
dbus_set_audio_input_device(const int index)
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_audio_input_device(
            configurationManagerProxy,
            index,
            &error);
    if(error)
    {
        ERROR("Failed to call set_audio_input_device() on ConfigurationManager: %s", error->message);
        g_error_free(error);
    }
}

/**
 * Get output device index and input device index
 */
    gchar**
dbus_get_current_audio_devices_index()
{
    gchar** array;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_current_audio_devices_index(
            configurationManagerProxy,
            &array,
            &error);
    if(error)
    {
        ERROR("Failed to call get_current_audio_devices_index() on ConfigurationManager: %s", error->message);
        g_error_free(error);
    }
    return array;
}

/**
 * Get index
 */
    int
dbus_get_audio_device_index(const gchar *name)
{
    int index;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_audio_device_index(
            configurationManagerProxy,
            name,
            &index,
            &error);
    if(error)
    {
        ERROR("Failed to call get_audio_device_index() on ConfigurationManager: %s", error->message);
        g_error_free(error);
    }
    return index;
}

/**
 * Get audio plugin
 */
    gchar*
dbus_get_current_audio_output_plugin()
{
    gchar* plugin="";
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_current_audio_output_plugin(
            configurationManagerProxy,
            &plugin,
            &error);
    if(error)
    {
        ERROR("Failed to call get_current_audio_output_plugin() on ConfigurationManager: %s", error->message);
        g_error_free(error);
    }
    return plugin;
}


    gchar*
dbus_get_ringtone_choice()
{
    gchar* tone;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_ringtone_choice(
            configurationManagerProxy,
            &tone,
            &error);
    if(error)
    {
        g_error_free(error);
    }
    return tone;
}

    void
dbus_set_ringtone_choice( const gchar* tone )
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_ringtone_choice(
            configurationManagerProxy,
            tone,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}

    int
dbus_is_ringtone_enabled()
{
    int res;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_is_ringtone_enabled(
            configurationManagerProxy,
            &res,
            &error);
    if(error)
    {
        g_error_free(error);
    }
    return res;
}

    void
dbus_ringtone_enabled()
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_ringtone_enabled(
            configurationManagerProxy,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}

    int
dbus_is_iax2_enabled()
{
    int res;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_is_iax2_enabled(
            configurationManagerProxy,
            &res,
            &error);
    if(error)
    {
        g_error_free(error);
    }
    return res;
}

    int
dbus_get_dialpad()
{
    int state;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_dialpad(
            configurationManagerProxy,
            &state,
            &error);
    if(error)
    {
        g_error_free(error);
    }
    return state;
}

    void
dbus_set_dialpad(  )
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_dialpad(
            configurationManagerProxy,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}

    int
dbus_get_searchbar()
{
    int state;
    GError* error = NULL;
    if(!org_sflphone_SFLphone_ConfigurationManager_get_searchbar( configurationManagerProxy, &state, &error))
    {
        if(error->domain == DBUS_GERROR && error->code == DBUS_GERROR_REMOTE_EXCEPTION)
        {
            ERROR ("Caught remote method (get_searchbar) exception  %s: %s", dbus_g_error_get_name(error), error->message);
        }
        else
        {
            ERROR("Error while calling get_searchbar: %s", error->message);
        }
        g_error_free (error);
        return -1;
    }
    else
    {
        return state;
    }
}

    void
dbus_set_searchbar(  )
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_searchbar(
            configurationManagerProxy,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}

    int
dbus_get_volume_controls()
{
    int state;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_volume_controls(
            configurationManagerProxy,
            &state,
            &error);
    if(error)
    {
        g_error_free(error);
    }
    return state;
}

    void
dbus_set_volume_controls(  )
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_volume_controls(
            configurationManagerProxy,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}


    void
dbus_set_record(const callable_obj_t * c)
{
    DEBUG("calling dbus_set_record on CallManager");
    DEBUG("CallID : %s", c->_callID);
    GError* error = NULL;
    org_sflphone_SFLphone_CallManager_set_recording (
            callManagerProxy,
            c->_callID,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}

    gboolean
dbus_get_is_recording(const callable_obj_t * c)
{
    DEBUG("calling dbus_get_is_recording on CallManager");
    GError* error = NULL;
    gboolean isRecording;
    org_sflphone_SFLphone_CallManager_get_is_recording (
            callManagerProxy,
            c->_callID,
            &isRecording,
            &error);
    if(error)
    {
        g_error_free(error);
    }
    //DEBUG("RECORDING: %i",isRecording);
    return isRecording;
}

    void
dbus_set_record_path(const gchar* path)
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_record_path (
            configurationManagerProxy,
            path,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}

    gchar*
dbus_get_record_path(void)
{
    GError* error = NULL;
    gchar *path;
    org_sflphone_SFLphone_ConfigurationManager_get_record_path (
            configurationManagerProxy,
            &path,
            &error);
    if(error)
    {
        g_error_free(error);
    }
    return path;
}

void dbus_set_history_limit (const guint days)
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_history_limit(
            configurationManagerProxy,
            days,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}

void dbus_set_history_enabled ()
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_history_enabled(
            configurationManagerProxy,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}

int dbus_get_history_enabled ()
{
    int state; 
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_history_enabled(
            configurationManagerProxy,
            &state,
            &error);
    if(error)
    {
        g_error_free(error);
    }
    return state;
}



guint dbus_get_history_limit (void)
{
    GError* error = NULL;
    gint days=30;
    org_sflphone_SFLphone_ConfigurationManager_get_history_limit(
            configurationManagerProxy,
            &days,
            &error);
    if(error)
    {
        g_error_free(error);
    }
    return (guint)days;
}

    void
dbus_start_hidden( void )
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_start_hidden(
            configurationManagerProxy,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}


    int
dbus_is_start_hidden( void )
{
    GError* error = NULL;
    int state;
    org_sflphone_SFLphone_ConfigurationManager_is_start_hidden(
            configurationManagerProxy,
            &state,
            &error);
    if(error)
    {
        g_error_free(error);
    }
    return state;
}

    int
dbus_popup_mode( void )
{
    GError* error = NULL;
    int state;
    org_sflphone_SFLphone_ConfigurationManager_popup_mode(
            configurationManagerProxy,
            &state,
            &error);
    if(error)
    {
        g_error_free(error);
    }
    return state;
}

    void
dbus_switch_popup_mode( void )
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_switch_popup_mode(
            configurationManagerProxy,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}

    void
dbus_set_notify( void )
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_notify(
            configurationManagerProxy,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}

    guint
dbus_get_notify( void )
{
    gint level;
    GError* error = NULL;
    if( !org_sflphone_SFLphone_ConfigurationManager_get_notify( configurationManagerProxy,&level, &error) )
    {
        if(error->domain == DBUS_GERROR && error->code == DBUS_GERROR_REMOTE_EXCEPTION)
        {
            ERROR ("Caught remote method (get_notify) exception  %s: %s", dbus_g_error_get_name(error), error->message);
        }
        else
        {
            ERROR ("Error while calling get_notify: %s", error->message);
        }
        g_error_free (error);
        return 0;
    }
    else{
        return (guint)level;
    }
}


    void
dbus_set_mail_notify( void )
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_mail_notify(
            configurationManagerProxy,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}

    guint
dbus_get_mail_notify( void )
{
    gint level;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_mail_notify(
            configurationManagerProxy,
            &level,
            &error);
    if(error)
    {
        ERROR("Error calling dbus_get_mail_notif_level");
        g_error_free(error);
    }

    return (guint)level;
}

    void
dbus_set_audio_manager( int api )
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_audio_manager(
            configurationManagerProxy,
            api,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}

    int
dbus_get_audio_manager( void )
{
    int api;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_audio_manager(
            configurationManagerProxy,
            &api,
            &error);
    if(error)
    {
        ERROR("Error calling dbus_get_audio_manager");
        g_error_free(error);
    }

    return api;
}

    void
dbus_set_pulse_app_volume_control( void )
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_pulse_app_volume_control(
            configurationManagerProxy,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}

    int
dbus_get_pulse_app_volume_control( void )
{
    int state;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_pulse_app_volume_control(
            configurationManagerProxy,
            &state,
            &error);
    return state;
}

    void
dbus_set_sip_port( const guint portNum  )
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_sip_port(
            configurationManagerProxy,
            portNum,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}

    guint
dbus_get_sip_port( void )
{
    GError* error = NULL;
    gint portNum;
    org_sflphone_SFLphone_ConfigurationManager_get_sip_port(
            configurationManagerProxy,
            &portNum,
            &error);
    if(error)
    {
        g_error_free(error);
    }
    return (guint)portNum;
}

gchar* dbus_get_stun_server (void)
{
    GError* error = NULL;
    gchar* server;
    org_sflphone_SFLphone_ConfigurationManager_get_stun_server(
            configurationManagerProxy,
            &server,
            &error);
    if(error)
    {
        g_error_free(error);
    }
    return server;
}

void dbus_set_stun_server( gchar* server)
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_stun_server(
            configurationManagerProxy,
            server,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}

gint dbus_stun_is_enabled (void)
{
    GError* error = NULL;
    gint stun;
    org_sflphone_SFLphone_ConfigurationManager_is_stun_enabled(
            configurationManagerProxy,
            &stun,
            &error);
    if(error)
    {
        g_error_free(error);
    }
    return stun;
}

void dbus_enable_stun (void)
{

    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_enable_stun(
            configurationManagerProxy,
            &error);
    if(error)
    {
        g_error_free(error);
    }
}

GHashTable* dbus_get_addressbook_settings (void) {

    GError *error = NULL;
    GHashTable *results = NULL;

    //DEBUG ("Calling org_sflphone_SFLphone_ConfigurationManager_get_addressbook_settings");

    org_sflphone_SFLphone_ConfigurationManager_get_addressbook_settings (configurationManagerProxy, &results, &error);
    if (error){
        ERROR ("Error calling org_sflphone_SFLphone_ConfigurationManager_get_addressbook_settings");
        g_error_free (error);
    }

    return results;
}

void dbus_set_addressbook_settings (GHashTable * settings){

    GError *error = NULL;

    DEBUG ("Calling org_sflphone_SFLphone_ConfigurationManager_set_addressbook_settings");

    org_sflphone_SFLphone_ConfigurationManager_set_addressbook_settings (configurationManagerProxy, settings, &error);
    if (error){
        ERROR ("Error calling org_sflphone_SFLphone_ConfigurationManager_set_addressbook_settings");
        g_error_free (error);
    }
}

gchar** dbus_get_addressbook_list (void) {

    GError *error = NULL;
    gchar** array;

    org_sflphone_SFLphone_ConfigurationManager_get_addressbook_list (configurationManagerProxy, &array, &error);
    if (error){
        ERROR ("Error calling org_sflphone_SFLphone_ConfigurationManager_get_addressbook_list");
        g_error_free (error);
    }

    return array;
}

void dbus_set_addressbook_list (const gchar** list){

    GError *error = NULL;

    org_sflphone_SFLphone_ConfigurationManager_set_addressbook_list(configurationManagerProxy, list, &error);
    if (error){
        ERROR ("Error calling org_sflphone_SFLphone_ConfigurationManager_set_addressbook_list");
        g_error_free (error);
    }
}

GHashTable* dbus_get_hook_settings (void) {

    GError *error = NULL;
    GHashTable *results = NULL;

    //DEBUG ("Calling org_sflphone_SFLphone_ConfigurationManager_get_addressbook_settings");

    org_sflphone_SFLphone_ConfigurationManager_get_hook_settings (configurationManagerProxy, &results, &error);
    if (error){
        ERROR ("Error calling org_sflphone_SFLphone_ConfigurationManager_get_hook_settings");
        g_error_free (error);
    }

    return results;
}

void dbus_set_hook_settings (GHashTable * settings){

    GError *error = NULL;

    org_sflphone_SFLphone_ConfigurationManager_set_hook_settings (configurationManagerProxy, settings, &error);
    if (error){
        ERROR ("Error calling org_sflphone_SFLphone_ConfigurationManager_set_hook_settings");
        g_error_free (error);
    }
}

GHashTable* dbus_get_call_details (const gchar *callID)
{
    GError *error = NULL;
    GHashTable *details = NULL;

    org_sflphone_SFLphone_CallManager_get_call_details (callManagerProxy, callID, &details, &error);
    if (error){
        ERROR ("Error calling org_sflphone_SFLphone_CallManager_get_call_details");
        g_error_free (error);
    }

    return details;
}

gchar** dbus_get_call_list (void)
{
    GError *error = NULL;
    gchar **list = NULL;

    org_sflphone_SFLphone_CallManager_get_call_list (callManagerProxy, &list, &error);
    if (error){
        ERROR ("Error calling org_sflphone_SFLphone_CallManager_get_call_list");
        g_error_free (error);
    }

    return list;
}

void dbus_set_accounts_order (const gchar* order) {

    GError *error = NULL;

    org_sflphone_SFLphone_ConfigurationManager_set_accounts_order (configurationManagerProxy, order, &error);
    if (error){
        ERROR ("Error calling org_sflphone_SFLphone_ConfigurationManager_set_accounts_order");
        g_error_free (error);
    }
}

GHashTable* dbus_get_history (void)
{
    GError *error = NULL;
    GHashTable *entries = NULL;

    org_sflphone_SFLphone_ConfigurationManager_get_history (configurationManagerProxy, &entries, &error);
    if (error){
        ERROR ("Error calling org_sflphone_SFLphone_CallManager_get_history");
        g_error_free (error);
    }

    return entries;
}

void dbus_set_history (GHashTable* entries)
{
    GError *error = NULL;

    org_sflphone_SFLphone_ConfigurationManager_set_history (configurationManagerProxy, entries, &error);
    if (error){
        ERROR ("Error calling org_sflphone_SFLphone_CallManager_set_history");
        g_error_free (error);
    }
}
