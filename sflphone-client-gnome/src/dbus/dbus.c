
conference_created_cb (DBusGProxy *proxy UNUSED, const gchar* confID, void * foo  UNUSED)
{
    DEBUG ("DBUS: Conference %s added", confID);

    conference_obj_t* new_conf;
    callable_obj_t* call;
    gchar* call_id;
    gchar** participants;
    gchar** part;

    create_new_conference (CONFERENCE_STATE_ACTIVE_ATACHED, confID, &new_conf);

    conferencelist_add (current_calls, new_conf);
    conferencelist_add (history, new_conf);

    participants = (gchar**) dbus_get_participant_list (new_conf->_confID);

    // Update conference list
    conference_participant_list_update (participants, new_conf);

    // Add conference ID in in each calls
    for (part = participants; *part; part++) {
        call_id = (gchar*) (*part);
        call = calllist_get_call (current_calls, call_id);

        // if a text widget is already created, disable it, use conference widget instead
        if (call->_im_widget) {
            im_widget_update_state (IM_WIDGET (call->_im_widget), FALSE);
        }

        // if one of these participant is currently recording, the whole conference will be recorded
        if(call->_state == CALL_STATE_RECORD) {
            new_conf->_state = CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD;
        }

        call->_confID = g_strdup (confID);
    }


    set_timestamp(&new_conf->_time_start);

    calltree_add_conference (current_calls, new_conf);
    calltree_add_conference (history, new_conf);

    calltree_display(current_calls);
}

static void
conference_removed_cb (DBusGProxy *proxy UNUSED, const gchar* confID, void * foo  UNUSED)
{
    DEBUG ("DBUS: Conference removed %s", confID);

    conference_obj_t * c = conferencelist_get (current_calls, confID);
    calltree_remove_conference (current_calls, c, NULL);

    GSList *participant = c->participant_list;
    callable_obj_t *call;

    // deactivate instant messaging window for this conference
    if (c->_im_widget)
        im_widget_update_state (IM_WIDGET (c->_im_widget), FALSE);

    // remove all participant for this conference
    while (participant) {

        call = calllist_get_call (current_calls, (const gchar *) (participant->data));

        if (call) {
            DEBUG ("DBUS: Remove participant %s", call->_callID);

            if (call->_confID) {
                g_free (call->_confID);
                call->_confID = NULL;
            }

            // if an instant messaging was previously disabled, enabled it
            if (call->_im_widget)
                im_widget_update_state (IM_WIDGET (call->_im_widget), TRUE);
        }

        participant = conference_next_participant (participant);
    }

    conferencelist_remove (current_calls, c->_confID);
}

static void
accounts_changed_cb (DBusGProxy *proxy UNUSED, void * foo  UNUSED)
{
    DEBUG ("Accounts changed");
    sflphone_fill_account_list();
    sflphone_fill_ip2ip_profile();
    account_list_config_dialog_fill();

    // Update the status bar in case something happened
    // Should fix ticket #1215
    status_bar_display_account();

    // Update the tooltip on the status icon
    statusicon_set_tooltip ();
}

static void
transfer_succeded_cb (DBusGProxy *proxy UNUSED, void * foo  UNUSED)
{
    DEBUG ("Transfer succeded\n");
    sflphone_display_transfer_status ("Transfer successfull");
}

static void
transfer_failed_cb (DBusGProxy *proxy UNUSED, void * foo  UNUSED)
{
    DEBUG ("Transfer failed\n");
    sflphone_display_transfer_status ("Transfer failed");
}

static void
secure_sdes_on_cb (DBusGProxy *proxy UNUSED, const gchar *callID, void *foo UNUSED)
{
    DEBUG ("SRTP using SDES is on");
    callable_obj_t *c = calllist_get_call (current_calls, callID);

    if (c) {
        sflphone_srtp_sdes_on (c);
        notify_secure_on (c);
    }

}

static void
secure_sdes_off_cb (DBusGProxy *proxy UNUSED, const gchar *callID, void *foo UNUSED)
{
    DEBUG ("SRTP using SDES is off");
    callable_obj_t *c = calllist_get_call (current_calls, callID);

    if (c) {
        sflphone_srtp_sdes_off (c);
        notify_secure_off (c);
    }
}

static void
secure_zrtp_on_cb (DBusGProxy *proxy UNUSED, const gchar* callID, const gchar* cipher,
                   void * foo  UNUSED)
{
    DEBUG ("SRTP using ZRTP is ON secure_on_cb");
    callable_obj_t * c = calllist_get_call (current_calls, callID);

    if (c) {
        c->_srtp_cipher = g_strdup (cipher);

        sflphone_srtp_zrtp_on (c);
        notify_secure_on (c);
    }
}

static void
secure_zrtp_off_cb (DBusGProxy *proxy UNUSED, const gchar* callID, void * foo  UNUSED)
{
    DEBUG ("SRTP using ZRTP is OFF");
    callable_obj_t * c = calllist_get_call (current_calls, callID);

    if (c) {
        sflphone_srtp_zrtp_off (c);
        notify_secure_off (c);
    }
}

static void
show_zrtp_sas_cb (DBusGProxy *proxy UNUSED, const gchar* callID, const gchar* sas,
                  const gboolean verified, void * foo  UNUSED)
{
    DEBUG ("Showing SAS");
    callable_obj_t * c = calllist_get_call (current_calls, callID);

    if (c) {
        sflphone_srtp_zrtp_show_sas (c, sas, verified);
    }
}

static void
confirm_go_clear_cb (DBusGProxy *proxy UNUSED, const gchar* callID, void * foo  UNUSED)
{
    DEBUG ("Confirm Go Clear request");
    callable_obj_t * c = calllist_get_call (current_calls, callID);

    if (c) {
        sflphone_confirm_go_clear (c);
    }
}

static void
zrtp_not_supported_cb (DBusGProxy *proxy UNUSED, const gchar* callID, void * foo  UNUSED)
{
    DEBUG ("ZRTP not supported on the other end");
    callable_obj_t * c = calllist_get_call (current_calls, callID);

    if (c) {
        sflphone_srtp_zrtp_not_supported (c);
        notify_zrtp_not_supported (c);
    }
}

static void
sip_call_state_cb (DBusGProxy *proxy UNUSED, const gchar* callID,
                   const gchar* description, const guint code, void * foo  UNUSED)
{
    callable_obj_t * c = NULL;
    c = calllist_get_call (current_calls, callID);
    if (c != NULL) {
        ERROR("DBUS: Error call is NULL in state changed");
        return;
    }

    sflphone_call_state_changed (c, description, code);
}

static void
error_alert (DBusGProxy *proxy UNUSED, int errCode, void * foo  UNUSED)
{
    ERROR ("Error notifying : (%i)", errCode);
    sflphone_throw_exception (errCode);
}

gboolean
dbus_connect (GError **error)
{
    connection = NULL;
    instanceProxy = NULL;

    g_type_init();

    connection = dbus_g_bus_get (DBUS_BUS_SESSION, error);

    if (connection == NULL)
        return FALSE;

    /* Create a proxy object for the "bus driver" (name "org.freedesktop.DBus") */

    instanceProxy = dbus_g_proxy_new_for_name (connection,
                    "org.sflphone.SFLphone", "/org/sflphone/SFLphone/Instance",
                    "org.sflphone.SFLphone.Instance");

    if (instanceProxy == NULL) {
        ERROR ("Failed to get proxy to Instance");
        return FALSE;
    }

    DEBUG ("DBus connected to Instance");

    callManagerProxy = dbus_g_proxy_new_for_name (connection,
                       "org.sflphone.SFLphone", "/org/sflphone/SFLphone/CallManager",
                       "org.sflphone.SFLphone.CallManager");
    g_assert (callManagerProxy != NULL);

    DEBUG ("DBus connected to CallManager");
    /* STRING STRING STRING Marshaller */
    /* Incoming call */
    dbus_g_object_register_marshaller (
        g_cclosure_user_marshal_VOID__STRING_STRING_STRING, G_TYPE_NONE,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (callManagerProxy, "newCallCreated", G_TYPE_STRING,
			     G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "newCallCreated",
				 G_CALLBACK (new_call_created_cb), NULL, NULL);
    dbus_g_proxy_add_signal (callManagerProxy, "incomingCall", G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "incomingCall",
                                 G_CALLBACK (incoming_call_cb), NULL, NULL);

    dbus_g_proxy_add_signal (callManagerProxy, "zrtpNegotiationFailed",
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "zrtpNegotiationFailed",
                                 G_CALLBACK (zrtp_negotiation_failed_cb), NULL, NULL);

    /* Current audio codec */
    dbus_g_object_register_marshaller (
        g_cclosure_user_marshal_VOID__STRING_STRING_STRING, G_TYPE_NONE,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (callManagerProxy, "currentSelectedAudioCodec",
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "currentSelectedAudioCodec",
                                 G_CALLBACK (current_selected_audio_codec), NULL, NULL);

    /* Register a marshaller for STRING,STRING */
    dbus_g_object_register_marshaller (
        g_cclosure_user_marshal_VOID__STRING_STRING, G_TYPE_NONE, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (callManagerProxy, "callStateChanged", G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "callStateChanged",
                                 G_CALLBACK (call_state_cb), NULL, NULL);

    dbus_g_object_register_marshaller (g_cclosure_user_marshal_VOID__STRING_INT,
                                       G_TYPE_NONE, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (callManagerProxy, "voiceMailNotify", G_TYPE_STRING,
                             G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "voiceMailNotify",
                                 G_CALLBACK (voice_mail_cb), NULL, NULL);

    dbus_g_proxy_add_signal (callManagerProxy, "incomingMessage", G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "incomingMessage",
                                 G_CALLBACK (incoming_message_cb), NULL, NULL);

    dbus_g_object_register_marshaller (
        g_cclosure_user_marshal_VOID__STRING_DOUBLE, G_TYPE_NONE, G_TYPE_STRING,
        G_TYPE_DOUBLE, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (callManagerProxy, "volumeChanged", G_TYPE_STRING,
                             G_TYPE_DOUBLE, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "volumeChanged",
                                 G_CALLBACK (volume_changed_cb), NULL, NULL);

    dbus_g_proxy_add_signal (callManagerProxy, "transferSucceded", G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "transferSucceded",
                                 G_CALLBACK (transfer_succeded_cb), NULL, NULL);

    dbus_g_proxy_add_signal (callManagerProxy, "transferFailed", G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "transferFailed",
                                 G_CALLBACK (transfer_failed_cb), NULL, NULL);

    /* Conference related callback */

    dbus_g_object_register_marshaller (g_cclosure_user_marshal_VOID__STRING,
                                       G_TYPE_NONE, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (callManagerProxy, "conferenceChanged", G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "conferenceChanged",
                                 G_CALLBACK (conference_changed_cb), NULL, NULL);

    dbus_g_proxy_add_signal (callManagerProxy, "conferenceCreated", G_TYPE_STRING,
                             G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "conferenceCreated",
                                 G_CALLBACK (conference_created_cb), NULL, NULL);

    dbus_g_proxy_add_signal (callManagerProxy, "conferenceRemoved", G_TYPE_STRING,
                             G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "conferenceRemoved",
                                 G_CALLBACK (conference_removed_cb), NULL, NULL);

    /* Security related callbacks */

    dbus_g_proxy_add_signal (callManagerProxy, "secureSdesOn", G_TYPE_STRING,
                             G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "secureSdesOn",
                                 G_CALLBACK (secure_sdes_on_cb), NULL, NULL);

    dbus_g_proxy_add_signal (callManagerProxy, "secureSdesOff", G_TYPE_STRING,
                             G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "secureSdesOff",
                                 G_CALLBACK (secure_sdes_off_cb), NULL, NULL);

    /* Register a marshaller for STRING,STRING,BOOL */
    dbus_g_object_register_marshaller (
        g_cclosure_user_marshal_VOID__STRING_STRING_BOOL, G_TYPE_NONE,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (callManagerProxy, "showSAS", G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "showSAS",
                                 G_CALLBACK (show_zrtp_sas_cb), NULL, NULL);

    dbus_g_proxy_add_signal (callManagerProxy, "secureZrtpOn", G_TYPE_STRING,
                             G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "secureZrtpOn",
                                 G_CALLBACK (secure_zrtp_on_cb), NULL, NULL);

    /* Register a marshaller for STRING*/
    dbus_g_object_register_marshaller (g_cclosure_user_marshal_VOID__STRING,
                                       G_TYPE_NONE, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (callManagerProxy, "secureZrtpOff", G_TYPE_STRING,
                             G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "secureZrtpOff",
                                 G_CALLBACK (secure_zrtp_off_cb), NULL, NULL);
    dbus_g_proxy_add_signal (callManagerProxy, "zrtpNotSuppOther", G_TYPE_STRING,
                             G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "zrtpNotSuppOther",
                                 G_CALLBACK (zrtp_not_supported_cb), NULL, NULL);
    dbus_g_proxy_add_signal (callManagerProxy, "confirmGoClear", G_TYPE_STRING,
                             G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "confirmGoClear",
                                 G_CALLBACK (confirm_go_clear_cb), NULL, NULL);

    /* VOID STRING STRING INT */
    dbus_g_object_register_marshaller (
        g_cclosure_user_marshal_VOID__STRING_STRING_INT, G_TYPE_NONE,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INVALID);

    dbus_g_proxy_add_signal (callManagerProxy, "sipCallStateChanged",
                             G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (callManagerProxy, "sipCallStateChanged",
                                 G_CALLBACK (sip_call_state_cb), NULL, NULL);

    configurationManagerProxy = dbus_g_proxy_new_for_name (connection,
                                "org.sflphone.SFLphone", "/org/sflphone/SFLphone/ConfigurationManager",
                                "org.sflphone.SFLphone.ConfigurationManager");
    g_assert (configurationManagerProxy != NULL);

    DEBUG ("DBus connected to ConfigurationManager");
    dbus_g_proxy_add_signal (configurationManagerProxy, "accountsChanged",
                             G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (configurationManagerProxy, "accountsChanged",
                                 G_CALLBACK (accounts_changed_cb), NULL, NULL);

    dbus_g_object_register_marshaller (g_cclosure_user_marshal_VOID__INT,
                                       G_TYPE_NONE, G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_add_signal (configurationManagerProxy, "errorAlert", G_TYPE_INT,
                             G_TYPE_INVALID);
    dbus_g_proxy_connect_signal (configurationManagerProxy, "errorAlert",
                                 G_CALLBACK (error_alert), NULL, NULL);

    /* Defines a default timeout for the proxies */
#if HAVE_DBUS_G_PROXY_SET_DEFAULT_TIMEOUT
    dbus_g_proxy_set_default_timeout (callManagerProxy, DEFAULT_DBUS_TIMEOUT);
    dbus_g_proxy_set_default_timeout (instanceProxy, DEFAULT_DBUS_TIMEOUT);
    dbus_g_proxy_set_default_timeout (configurationManagerProxy, DEFAULT_DBUS_TIMEOUT);
#endif

    return TRUE;
}

void
dbus_clean()
{
    g_object_unref (callManagerProxy);
    g_object_unref (configurationManagerProxy);
    g_object_unref (instanceProxy);
}

void
dbus_hold (const callable_obj_t * c)
{
    DEBUG ("dbus_hold %s\n", c->_callID);

    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_hold (callManagerProxy, c->_callID, &error);

    if (error) {
        ERROR ("Failed to call hold() on CallManager: %s",
               error->message);
        g_error_free (error);
    }
}

void
dbus_unhold (const callable_obj_t * c)
{
    DEBUG ("dbus_unhold %s\n", c->_callID);

    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_unhold (callManagerProxy, c->_callID, &error);

    if (error) {
        ERROR ("Failed to call unhold() on CallManager: %s",
               error->message);
        g_error_free (error);
    }
}

void
dbus_hold_conference (const conference_obj_t * c)
{
    DEBUG ("dbus_hold_conference %s\n", c->_confID);

    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_hold_conference (callManagerProxy,
            c->_confID, &error);

    if (error) {
        ERROR ("Failed to call hold() on CallManager: %s",
               error->message);
        g_error_free (error);
    }
}

void
dbus_unhold_conference (const conference_obj_t * c)
{
    DEBUG ("dbus_unhold_conference %s\n", c->_confID);

    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_unhold_conference (callManagerProxy,
            c->_confID, &error);

    if (error) {
        ERROR ("Failed to call unhold() on CallManager: %s",
               error->message);
        g_error_free (error);
    }
}

void
dbus_hang_up (const callable_obj_t * c)
{
    DEBUG ("dbus_hang_up %s\n", c->_callID);

    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_hang_up (callManagerProxy, c->_callID,
            &error);

    if (error) {
        ERROR ("Failed to call hang_up() on CallManager: %s",
               error->message);
        g_error_free (error);
    }
}

void
dbus_hang_up_conference (const conference_obj_t * c)
{
    DEBUG ("dbus_hang_up_conference %s\n", c->_confID);

    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_hang_up_conference (callManagerProxy,
            c->_confID, &error);

    if (error) {
        ERROR ("Failed to call hang_up() on CallManager: %s",
               error->message);
        g_error_free (error);
    }
}

void
dbus_transfert (const callable_obj_t * c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_transfert (callManagerProxy, c->_callID,
            c->_trsft_to, &error);

    if (error) {
        ERROR ("Failed to call transfert() on CallManager: %s",
               error->message);
        g_error_free (error);
    }
}

void
dbus_attended_transfer (const callable_obj_t *transfer, const callable_obj_t *target)
{
    DEBUG("DBUS: Attended tramsfer %s to %s", transfer->_callID, target->_callID);

    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_attended_transfer (callManagerProxy, transfer->_callID,
        target->_callID, &error);

    if(error) {
        ERROR("Failed to call transfer() on CallManager: %s",
              error->message);
        g_error_free(error);
    }
}

void
dbus_accept (const callable_obj_t * c)
{
#if GTK_CHECK_VERSION(2,10,0)
    status_tray_icon_blink (FALSE);
#endif

    DEBUG ("dbus_accept %s\n", c->_callID);

    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_accept (callManagerProxy, c->_callID, &error);

    if (error) {
        ERROR ("Failed to call accept(%s) on CallManager: %s", c->_callID,
               (error->message == NULL ? g_quark_to_string (error->domain) : error->message));
        g_error_free (error);
    }
}

void
dbus_refuse (const callable_obj_t * c)
{
#if GTK_CHECK_VERSION(2,10,0)
    status_tray_icon_blink (FALSE);
#endif

    DEBUG ("dbus_refuse %s\n", c->_callID);

    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_refuse (callManagerProxy, c->_callID, &error);

    if (error) {
        ERROR ("Failed to call refuse() on CallManager: %s",
               error->message);
        g_error_free (error);
    }
}

void
dbus_place_call (const callable_obj_t * c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_place_call (callManagerProxy, c->_accountID,
            c->_callID, c->_peer_number, &error);

    if (error) {
        ERROR ("Failed to call placeCall() on CallManager: %s",
               error->message);
        g_error_free (error);
    }
}

gchar**
dbus_account_list()
{
    GError *error = NULL;
    char ** array;

    if (!org_sflphone_SFLphone_ConfigurationManager_get_account_list (
                configurationManagerProxy, &array, &error)) {
        if (error->domain == DBUS_GERROR && error->code
                == DBUS_GERROR_REMOTE_EXCEPTION) {
            ERROR ("Caught remote method (get_account_list) exception  %s: %s", dbus_g_error_get_name (error), error->message);
        } else {
            ERROR ("Error while calling get_account_list: %s", error->message);
        }

        g_error_free (error);
        return NULL;
    } else {
        DEBUG ("DBus called get_account_list() on ConfigurationManager");
        return array;
    }
}

GHashTable*
dbus_get_account_details (gchar * accountID)
{
    GError *error = NULL;
    GHashTable * details;

    DEBUG ("Dbus: Get account detail accountid %s", accountID);

    if (!org_sflphone_SFLphone_ConfigurationManager_get_account_details (
                configurationManagerProxy, accountID, &details, &error)) {
        if (error->domain == DBUS_GERROR && error->code
                == DBUS_GERROR_REMOTE_EXCEPTION) {
            ERROR ("Caught remote method (get_account_details) exception  %s: %s", dbus_g_error_get_name (error), error->message);
        } else {
            ERROR ("Error while calling get_account_details: %s", error->message);
        }

        g_error_free (error);
        return NULL;
    } else {
        return details;
    }
}

void
dbus_set_credential (account_t *a, int index)
{
    DEBUG ("Sending credential %d to server", index);
    GError *error = NULL;
    GHashTable * credential = g_ptr_array_index (a->credential_information, index);

    if (credential == NULL) {
        DEBUG ("Credential %d was deleted", index);
    } else {
        org_sflphone_SFLphone_ConfigurationManager_set_credential (
            configurationManagerProxy, a->accountID, index, credential, &error);
    }

    if (error) {
        ERROR ("Failed to call set_credential() on ConfigurationManager: %s",
               error->message);
        g_error_free (error);
    }
}
void
dbus_delete_all_credential (account_t *a)
{
    DEBUG ("Deleting all credentials\n");
    GError *error = NULL;

    org_sflphone_SFLphone_ConfigurationManager_delete_all_credential (
        configurationManagerProxy, a->accountID, &error);

    if (error) {
        ERROR ("Failed to call deleteAllCredential on ConfigurationManager: %s",
               error->message);
        g_error_free (error);
    }
}

int
dbus_get_number_of_credential (gchar * accountID)
{
    GError *error = NULL;
    int number = 0;

    DEBUG ("Getting number of credential for account %s", accountID);

    if (!org_sflphone_SFLphone_ConfigurationManager_get_number_of_credential (
                configurationManagerProxy, accountID, &number, &error)) {
        if (error->domain == DBUS_GERROR && error->code
                == DBUS_GERROR_REMOTE_EXCEPTION) {
            ERROR ("Caught remote method (get_account_details) exception  %s: %s", dbus_g_error_get_name (error), error->message);
        } else {
            ERROR ("Error while calling get_account_details: %s", error->message);
        }

        g_error_free (error);
        return 0;
    } else {
        DEBUG ("%d credential(s) found for account %s", number, accountID);
        return number;
    }
}

GHashTable*
dbus_get_credential (gchar * accountID, int index)
{
    GError *error = NULL;
    GHashTable * details;

    if (!org_sflphone_SFLphone_ConfigurationManager_get_credential (
                configurationManagerProxy, accountID, index, &details, &error)) {
        if (error->domain == DBUS_GERROR && error->code
                == DBUS_GERROR_REMOTE_EXCEPTION) {
            ERROR ("Caught remote method (get_account_details) exception  %s: %s", dbus_g_error_get_name (error), error->message);
        } else {
            ERROR ("Error while calling get_account_details: %s", error->message);
        }

        g_error_free (error);
        return NULL;
    } else {
        return details;
    }
}

GHashTable*
dbus_get_ip2_ip_details (void)
{
    GError *error = NULL;
    GHashTable * details;

    if (!org_sflphone_SFLphone_ConfigurationManager_get_ip2_ip_details (
                configurationManagerProxy, &details, &error)) {
        if (error->domain == DBUS_GERROR && error->code
                == DBUS_GERROR_REMOTE_EXCEPTION) {
            ERROR ("Caught remote method (get_ip2_ip_details) exception  %s: %s", dbus_g_error_get_name (error), error->message);
        } else {
            ERROR ("Error while calling get_ip2_ip_details: %s", error->message);
        }

        g_error_free (error);
        return NULL;
    } else {
        return details;
    }
}

void
dbus_set_ip2ip_details (GHashTable * properties)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_ip2_ip_details (
        configurationManagerProxy, properties, &error);

    if (error) {
        ERROR ("Failed to call set_ip_2ip_details() on ConfigurationManager: %s",
               error->message);
        g_error_free (error);
    }
}

void
dbus_send_register (gchar* accountID, const guint enable)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_send_register (
        configurationManagerProxy, accountID, enable, &error);

    if (error) {
        ERROR ("Failed to call send_register() on ConfigurationManager: %s",
               error->message);
        g_error_free (error);
    }
}

void
dbus_remove_account (gchar * accountID)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_remove_account (
        configurationManagerProxy, accountID, &error);

    if (error) {
        ERROR ("Failed to call remove_account() on ConfigurationManager: %s",
               error->message);
        g_error_free (error);
    }
}

void
dbus_set_account_details (account_t *a)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_account_details (
        configurationManagerProxy, a->accountID, a->properties, &error);

    if (error) {
        ERROR ("Failed to call set_account_details() on ConfigurationManager: %s",
               error->message);
        g_error_free (error);
    }
}
gchar*
dbus_add_account (account_t *a)
{
    gchar* accountId;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_add_account (
        configurationManagerProxy, a->properties, &accountId, &error);

    if (error) {
        ERROR ("Failed to call add_account() on ConfigurationManager: %s",
               error->message);
        g_error_free (error);
    }

    return accountId;
}

void
dbus_set_volume (const gchar * device, gdouble value)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_set_volume (callManagerProxy, device, value,
            &error);

    if (error) {
        ERROR ("Failed to call set_volume() on callManagerProxy: %s",
               error->message);
        g_error_free (error);
    }
}

gdouble
dbus_get_volume (const gchar * device)
{
    gdouble value;
    GError *error = NULL;

    org_sflphone_SFLphone_CallManager_get_volume (callManagerProxy, device,
            &value, &error);

    if (error) {
        ERROR ("Failed to call get_volume() on callManagerProxy: %s",
               error->message);
        g_error_free (error);
    }

    return value;
}

void
dbus_play_dtmf (const gchar * key)
{
    GError *error = NULL;

    org_sflphone_SFLphone_CallManager_play_dt_mf (callManagerProxy, key, &error);

    if (error) {
        ERROR ("Failed to call playDTMF() on callManagerProxy: %s",
               error->message);
        g_error_free (error);
    }
}

void
dbus_start_tone (const int start, const guint type)
{
    GError *error = NULL;

    org_sflphone_SFLphone_CallManager_start_tone (callManagerProxy, start, type,
            &error);

    if (error) {
        ERROR ("Failed to call startTone() on callManagerProxy: %s",
               error->message);
        g_error_free (error);
    }
}

gboolean
dbus_register (int pid, gchar *name, GError **error)
{
    return org_sflphone_SFLphone_Instance_register (instanceProxy, pid, name, error);
}

void
dbus_unregister (int pid)
{
    GError *error = NULL;

    org_sflphone_SFLphone_Instance_unregister (instanceProxy, pid, &error);

    if (error) {
        ERROR ("Failed to call unregister() on instanceProxy: %s",
               error->message);
        g_error_free (error);
    }
}

gchar**
dbus_audio_codec_list()
{

    GError *error = NULL;
    gchar** array = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_audio_codec_list (
        configurationManagerProxy, &array, &error);

    if (error) {
        ERROR ("Failed to call get_audio_codec_list() on ConfigurationManager: %s",
               error->message);
        g_error_free (error);
    }

    return array;
}

gchar**
dbus_audio_codec_details (int payload)
{

    GError *error = NULL;
    gchar ** array;
    org_sflphone_SFLphone_ConfigurationManager_get_audio_codec_details (
        configurationManagerProxy, payload, &array, &error);

    if (error) {
        ERROR ("Failed to call get_audio_codec_details() on ConfigurationManager: %s",
               error->message);
        g_error_free (error);
    }

    return array;
}

gchar*
dbus_get_current_audio_codec_name (const callable_obj_t * c)
{

    gchar* codecName = "";
    GError* error = NULL;

    org_sflphone_SFLphone_CallManager_get_current_audio_codec_name (callManagerProxy,
            c->_callID, &codecName, &error);

    if (error) {
        g_error_free (error);
    }

    DEBUG ("%s: codecName : %s", __PRETTY_FUNCTION__, codecName);

    return codecName;
}

gchar**
dbus_get_active_audio_codec_list (gchar *accountID)
{

    gchar ** array;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_active_audio_codec_list (
        configurationManagerProxy, accountID, &array, &error);

    if (error) {
        ERROR ("Failed to call get_active_audio_codec_list() on ConfigurationManager: %s",
               error->message);
        g_error_free (error);
    }

    return array;
}

void
dbus_set_active_audio_codec_list (const gchar** list, const gchar *accountID)
{

    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_active_audio_codec_list (
        configurationManagerProxy, list, accountID, &error);

    if (error) {
        ERROR ("Failed to call set_active_audio_codec_list() on ConfigurationManager: %s",
               error->message);
        g_error_free (error);
    }
}


/**
 * Get a list of output supported audio plugins
 */
gchar**
dbus_get_audio_plugin_list()
{
    gchar** array;
    GError* error = NULL;

    if (!org_sflphone_SFLphone_ConfigurationManager_get_audio_plugin_list (
                configurationManagerProxy, &array, &error)) {
        if (error->domain == DBUS_GERROR && error->code
                == DBUS_GERROR_REMOTE_EXCEPTION) {
            ERROR ("Caught remote method (get_output_plugin_list) exception  %s: %s", dbus_g_error_get_name (error), error->message);
        } else {
            ERROR ("Error while calling get_out_plugin_list: %s", error->message);
        }

        g_error_free (error);
        return NULL;
    } else {
        return array;
    }
}

void
dbus_set_audio_plugin (gchar* audioPlugin)
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_audio_plugin (
        configurationManagerProxy, audioPlugin, &error);

    if (error) {
        ERROR ("Failed to call set_audio_plugin() on ConfigurationManager: %s", error->message);
        g_error_free (error);
    }
}

/**
 * Get all output devices index supported by current audio manager
 */
gchar**
dbus_get_audio_output_device_list()
{
    gchar** array;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_audio_output_device_list (
        configurationManagerProxy, &array, &error);

    if (error) {
        ERROR ("Failed to call get_audio_output_device_list() on ConfigurationManager: %s", error->message);
        g_error_free (error);
    }

    return array;
}

/**
 * Set audio output device from its index
 */
void
dbus_set_audio_output_device (const int index)
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_audio_output_device (
        configurationManagerProxy, index, &error);

    if (error) {
        ERROR ("Failed to call set_audio_output_device() on ConfigurationManager: %s", error->message);
        g_error_free (error);
    }
}

/**
 * Set audio input device from its index
 */
void
dbus_set_audio_input_device (const int index)
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_audio_input_device (
        configurationManagerProxy, index, &error);

    if (error) {
        ERROR ("Failed to call set_audio_input_device() on ConfigurationManager: %s", error->message);
        g_error_free (error);
    }
}

/**
 * Set adio ringtone device from its index
 */
void
dbus_set_audio_ringtone_device (const int index)
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_audio_ringtone_device (
        configurationManagerProxy, index, &error);

    if (error) {
        ERROR ("Failed to call set_audio_ringtone_device() on ConfigurationManager: %s", error->message);
        g_error_free (error);
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
    org_sflphone_SFLphone_ConfigurationManager_get_audio_input_device_list (
        configurationManagerProxy, &array, &error);

    if (error) {
        ERROR ("Failed to call get_audio_input_device_list() on ConfigurationManager: %s", error->message);
        g_error_free (error);
    }

    return array;
}

/**
 * Get output device index and input device index
 */
gchar**
dbus_get_current_audio_devices_index()
{
    gchar** array;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_current_audio_devices_index (
        configurationManagerProxy, &array, &error);

    if (error) {
        ERROR ("Failed to call get_current_audio_devices_index() on ConfigurationManager: %s", error->message);
        g_error_free (error);
    }

    return array;
}

/**
 * Get index
 */
int
dbus_get_audio_device_index (const gchar *name)
{
    int index;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_audio_device_index (
        configurationManagerProxy, name, &index, &error);

    if (error) {
        ERROR ("Failed to call get_audio_device_index() on ConfigurationManager: %s", error->message);
        g_error_free (error);
    }

    return index;
}

/**
 * Get audio plugin
 */
gchar*
dbus_get_current_audio_output_plugin()
{
    gchar* plugin = "";
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_current_audio_output_plugin (
        configurationManagerProxy, &plugin, &error);

    if (error) {
        ERROR ("Failed to call get_current_audio_output_plugin() on ConfigurationManager: %s", error->message);
        g_error_free (error);
    }

    return plugin;
}


/**
 * Get noise reduction state
 */
gchar*
dbus_get_noise_suppress_state()
{
    gchar* state = "";
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_noise_suppress_state (configurationManagerProxy, &state, &error);

    if (error) {
        ERROR ("DBus: Failed to call get_noise_suppress_state() on ConfigurationManager: %s", error->message);
        g_error_free (error);
    }

    return state;
}

/**
 * Set noise reduction state
 */
void
dbus_set_noise_suppress_state (gchar* state)
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_noise_suppress_state (
        configurationManagerProxy, state, &error);

    if (error) {
        ERROR ("Failed to call set_noise_suppress_state() on ConfigurationManager: %s", error->message);
        g_error_free (error);
    }
}

gchar *
dbus_get_echo_cancel_state(void)
{
    GError *error = NULL;
    gchar *state = "";
    org_sflphone_SFLphone_ConfigurationManager_get_echo_cancel_state(configurationManagerProxy, &state, &error);

    if(error) {
        ERROR("DBus: Failed to call get_echo_cancel_state() on ConfigurationManager: %s", error->message);
        g_error_free(error);
    }

    return state;
}

void
dbus_set_echo_cancel_state(gchar *state)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_echo_cancel_state(configurationManagerProxy, state, &error);

    if(error) {
        ERROR("DBus: Failed to call set_echo_cancel_state() on ConfigurationManager: %s", error->message);
        g_error_free(error);
    }
}

int
dbus_get_echo_cancel_tail_length(void)
{
    GError *error = NULL;
    int length;

    org_sflphone_SFLphone_ConfigurationManager_get_echo_cancel_tail_length(configurationManagerProxy, &length, &error);

    if(error) {
        ERROR("DBus: Failed to call get_echo_cancel_tail_length() on ConfigurationManager: %s", error->message);
        g_error_free(error);
    }

    return length;
}

void
dbus_set_echo_cancel_tail_length(int length)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_echo_cancel_tail_length(configurationManagerProxy, length, &error);

    if(error) {
        ERROR("DBus: Failed to call get_echo_cancel_state() on ConfigurationManager: %s", error->message);
        g_error_free(error);
    }
}

int
dbus_get_echo_cancel_delay(void)
{
    GError *error = NULL;
    int delay;

    org_sflphone_SFLphone_ConfigurationManager_get_echo_cancel_delay(configurationManagerProxy, &delay, &error);

    if(error) {
        ERROR("DBus: Failed to call get_echo_cancel_tail_length() on ConfigurationManager: %s", error->message);
        g_error_free(error);
    }

    return delay;
}

void
dbus_set_echo_cancel_delay(int delay)
{
    GError *error = NULL;

    org_sflphone_SFLphone_ConfigurationManager_set_echo_cancel_delay(configurationManagerProxy, delay, &error);

    if(error) {
        ERROR("DBus: Failed to call get_echo_cancel_delay() on ConfigurationManager: %s", error->message);
        g_error_free(error);
    }
}

gchar*
dbus_get_ringtone_choice (const gchar *accountID)
{
    gchar* tone;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_ringtone_choice (
        configurationManagerProxy, accountID, &tone, &error);

    if (error) {
        g_error_free (error);
    }

    return tone;
}

void
dbus_set_ringtone_choice (const gchar *accountID, const gchar* tone)
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_ringtone_choice (
        configurationManagerProxy, accountID, tone, &error);

    if (error) {
        g_error_free (error);
    }
}

int
dbus_is_ringtone_enabled (const gchar *accountID)
{
    int res;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_is_ringtone_enabled (
        configurationManagerProxy, accountID, &res, &error);

    if (error) {
        g_error_free (error);
    }

    return res;
}

void
dbus_ringtone_enabled (const gchar *accountID)
{
    DEBUG ("DBUS: Ringtone enabled %s", accountID);

    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_ringtone_enabled (
        configurationManagerProxy, accountID, &error);

    if (error) {
        g_error_free (error);
    }
}

gboolean
dbus_is_md5_credential_hashing()
{
    int res;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_is_md5_credential_hashing (
        configurationManagerProxy, &res, &error);

    if (error) {
        g_error_free (error);
    }

    return res;
}

void
dbus_set_md5_credential_hashing (gboolean enabled)
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_md5_credential_hashing (
        configurationManagerProxy, enabled, &error);

    if (error) {
        g_error_free (error);
    }
}

int
dbus_is_iax2_enabled()
{
    int res;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_is_iax2_enabled (
        configurationManagerProxy, &res, &error);

    if (error) {
        g_error_free (error);
    }

    return res;
}

void
dbus_join_participant (const gchar* sel_callID, const gchar* drag_callID)
{

    DEBUG ("DBUS: Join participant %s and %s\n", sel_callID, drag_callID);

    GError* error = NULL;

    org_sflphone_SFLphone_CallManager_join_participant (callManagerProxy,
            sel_callID, drag_callID, &error);

    if (error) {
        g_error_free (error);
    }

}

void
dbus_create_conf_from_participant_list(const gchar **list) {

    GError *error = NULL;

    DEBUG("DBUS: Create conference from participant list");

    org_sflphone_SFLphone_CallManager_create_conf_from_participant_list(callManagerProxy,
	list, &error);

    if(error) {
	DEBUG("DBUS: Error: %s", error->message);
	g_error_free(error);
    }
}  

void
dbus_add_participant (const gchar* callID, const gchar* confID)
{

    DEBUG ("DBUS: Add participant %s to %s\n", callID, confID);

    GError* error = NULL;

    org_sflphone_SFLphone_CallManager_add_participant (callManagerProxy, callID,
            confID, &error);

    if (error) {
        g_error_free (error);
    }

}

void
dbus_add_main_participant (const gchar* confID)
{
    DEBUG ("DBUS: Add main participant %s\n", confID);

    GError* error = NULL;

    org_sflphone_SFLphone_CallManager_add_main_participant (callManagerProxy,
            confID, &error);

    if (error) {
        g_error_free (error);
    }
}

void
dbus_detach_participant (const gchar* callID)
{

    DEBUG ("DBUS: Detach participant %s\n", callID);

    GError* error = NULL;
    org_sflphone_SFLphone_CallManager_detach_participant (callManagerProxy,
            callID, &error);

    if (error) {
        g_error_free (error);
    }

}

void
dbus_join_conference (const gchar* sel_confID, const gchar* drag_confID)
{

    DEBUG ("dbus_join_conference %s and %s\n", sel_confID, drag_confID);

    GError* error = NULL;

    org_sflphone_SFLphone_CallManager_join_conference (callManagerProxy,
            sel_confID, drag_confID, &error);

    if (error) {
        g_error_free (error);
    }

}

void
dbus_set_record (const gchar* id)
{
    DEBUG ("Dbus: dbus_set_record %s", id);

    GError* error = NULL;
    org_sflphone_SFLphone_CallManager_set_recording (callManagerProxy, id, &error);

    if (error) {
        g_error_free (error);
    }
}

gboolean
dbus_get_is_recording (const callable_obj_t * c)
{
    DEBUG ("Dbus: dbus_get_is_recording %s", c->_callID);
    GError* error = NULL;
    gboolean isRecording;
    org_sflphone_SFLphone_CallManager_get_is_recording (callManagerProxy,
            c->_callID, &isRecording, &error);

    if (error) {
        g_error_free (error);
    }

    //DEBUG("RECORDING: %i",isRecording);
    return isRecording;
}

void
dbus_set_record_path (const gchar* path)
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_record_path (
        configurationManagerProxy, path, &error);

    if (error) {
        g_error_free (error);
    }
}

gchar*
dbus_get_record_path (void)
{
    GError* error = NULL;
    gchar *path;
    org_sflphone_SFLphone_ConfigurationManager_get_record_path (
        configurationManagerProxy, &path, &error);

    if (error) {
        g_error_free (error);
    }

    return path;
}

void dbus_set_is_always_recording(const gboolean alwaysRec)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_is_always_recording(
        configurationManagerProxy, alwaysRec, &error);

    if(error) {
        ERROR("DBUS: Could not set isAlwaysRecording");
        g_error_free(error);
    }
}

gboolean dbus_get_is_always_recording(void)
{
    GError *error = NULL;
    int alwaysRec;
    org_sflphone_SFLphone_ConfigurationManager_get_is_always_recording(
        configurationManagerProxy, &alwaysRec, &error);

    if(error) {
        ERROR("DBUS: Could not get isAlwaysRecording");
        g_error_free(error);
    }

    return alwaysRec;
}

void
dbus_set_history_limit (const guint days)
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_history_limit (
        configurationManagerProxy, days, &error);

    if (error) {
        g_error_free (error);
    }
}

guint
dbus_get_history_limit (void)
{
    GError* error = NULL;
    gint days = 30;
    org_sflphone_SFLphone_ConfigurationManager_get_history_limit (
        configurationManagerProxy, &days, &error);

    if (error) {
        g_error_free (error);
    }

    return (guint) days;
}

void
dbus_set_audio_manager (int api)
{
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_audio_manager (
        configurationManagerProxy, api, &error);

    if (error) {
        g_error_free (error);
    }
}

int
dbus_get_audio_manager (void)
{
    int api;
    GError* error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_audio_manager (
        configurationManagerProxy, &api, &error);

    if (error) {
        ERROR ("Error calling dbus_get_audio_manager");
        g_error_free (error);
    }

    return api;
}

/*
   void
   dbus_set_sip_address( const gchar* address )
   {
   GError* error = NULL;
   org_sflphone_SFLphone_ConfigurationManager_set_sip_address(
   configurationManagerProxy,
   address,
   &error);
   if(error)
   {
   g_error_free(error);
   }
   }
 */

/*

   gint
   dbus_get_sip_address( void )
   {
   GError* error = NULL;
   gint address;
   org_sflphone_SFLphone_ConfigurationManager_get_sip_address(
   configurationManagerProxy,
   &address,
   &error);
   if(error)
   {
   g_error_free(error);
   }
   return address;
   }
 */

GHashTable*
dbus_get_addressbook_settings (void)
{

    GError *error = NULL;
    GHashTable *results = NULL;

    //DEBUG ("Calling org_sflphone_SFLphone_ConfigurationManager_get_addressbook_settings");

    org_sflphone_SFLphone_ConfigurationManager_get_addressbook_settings (
        configurationManagerProxy, &results, &error);

    if (error) {
        ERROR ("Error calling org_sflphone_SFLphone_ConfigurationManager_get_addressbook_settings");
        g_error_free (error);
    }

    return results;
}

void
dbus_set_addressbook_settings (GHashTable * settings)
{

    GError *error = NULL;

    DEBUG ("Calling org_sflphone_SFLphone_ConfigurationManager_set_addressbook_settings");

    org_sflphone_SFLphone_ConfigurationManager_set_addressbook_settings (
        configurationManagerProxy, settings, &error);

    if (error) {
        ERROR ("Error calling org_sflphone_SFLphone_ConfigurationManager_set_addressbook_settings");
        g_error_free (error);
    }
}

gchar**
dbus_get_addressbook_list (void)
{

    GError *error = NULL;
    gchar** array = NULL;

    org_sflphone_SFLphone_ConfigurationManager_get_addressbook_list (
        configurationManagerProxy, &array, &error);

    if (error) {
        ERROR ("Error calling org_sflphone_SFLphone_ConfigurationManager_get_addressbook_list");
        g_error_free (error);
    }

    return array;
}

void
dbus_set_addressbook_list (const gchar** list)
{

    GError *error = NULL;

    org_sflphone_SFLphone_ConfigurationManager_set_addressbook_list (
        configurationManagerProxy, list, &error);

    if (error) {
        ERROR ("Error calling org_sflphone_SFLphone_ConfigurationManager_set_addressbook_list");
        g_error_free (error);
    }
}

GHashTable*
dbus_get_hook_settings (void)
{

    GError *error = NULL;
    GHashTable *results = NULL;

    //DEBUG ("Calling org_sflphone_SFLphone_ConfigurationManager_get_addressbook_settings");

    org_sflphone_SFLphone_ConfigurationManager_get_hook_settings (
        configurationManagerProxy, &results, &error);

    if (error) {
        ERROR ("Error calling org_sflphone_SFLphone_ConfigurationManager_get_hook_settings");
        g_error_free (error);
    }

    return results;
}

void
dbus_set_hook_settings (GHashTable * settings)
{

    GError *error = NULL;

    org_sflphone_SFLphone_ConfigurationManager_set_hook_settings (
        configurationManagerProxy, settings, &error);

    if (error) {
        ERROR ("Error calling org_sflphone_SFLphone_ConfigurationManager_set_hook_settings");
        g_error_free (error);
    }
}

GHashTable*
dbus_get_call_details (const gchar *callID)
{
    GError *error = NULL;
    GHashTable *details = NULL;

    org_sflphone_SFLphone_CallManager_get_call_details (callManagerProxy, callID,
            &details, &error);

    if (error) {
        ERROR ("Error calling org_sflphone_SFLphone_CallManager_get_call_details");
        g_error_free (error);
    }

    return details;
}

gchar**
dbus_get_call_list (void)
{
    GError *error = NULL;
    gchar **list = NULL;

    org_sflphone_SFLphone_CallManager_get_call_list (callManagerProxy, &list,
            &error);

    if (error) {
        ERROR ("Error calling org_sflphone_SFLphone_CallManager_get_call_list");
        g_error_free (error);
    }

    return list;
}

gchar**
dbus_get_conference_list (void)
{
    GError *error = NULL;
    gchar **list = NULL;

    org_sflphone_SFLphone_CallManager_get_conference_list (callManagerProxy,
            &list, &error);

    if (error) {
        ERROR ("Error calling org_sflphone_SFLphone_CallManager_get_conference_list");
        g_error_free (error);
    }

    return list;
}

gchar**
dbus_get_participant_list (const gchar *confID)
{
    GError *error = NULL;
    char **list = NULL;

    DEBUG ("DBUS: Get conference %s participant list", confID);

    org_sflphone_SFLphone_CallManager_get_participant_list (callManagerProxy,
            confID, &list, &error);

    if (error) {
        ERROR ("Error calling org_sflphone_SFLphone_CallManager_get_participant_list");
        g_error_free (error);
    }

    return list;
}

GHashTable*
dbus_get_conference_details (const gchar *confID)
{
    GError *error = NULL;
    GHashTable *details = NULL;

    org_sflphone_SFLphone_CallManager_get_conference_details (callManagerProxy,
            confID, &details, &error);

    if (error) {
        ERROR ("Error calling org_sflphone_SFLphone_CallManager_get_conference_details");
        g_error_free (error);
    }

    return details;
}

void
dbus_set_accounts_order (const gchar* order)
{

    GError *error = NULL;

    org_sflphone_SFLphone_ConfigurationManager_set_accounts_order (
        configurationManagerProxy, order, &error);

    if (error) {
        ERROR ("Error calling org_sflphone_SFLphone_ConfigurationManager_set_accounts_order");
        g_error_free (error);
    }
}

GHashTable*
dbus_get_history (void)
{
    GError *error = NULL;
    GHashTable *entries = NULL;

    org_sflphone_SFLphone_ConfigurationManager_get_history (
        configurationManagerProxy, &entries, &error);

    if (error) {
        ERROR ("Error calling org_sflphone_SFLphone_CallManager_get_history");
        g_error_free (error);
    }

    return entries;
}

void
dbus_set_history (GHashTable* entries)
{
    GError *error = NULL;

    org_sflphone_SFLphone_ConfigurationManager_set_history (
        configurationManagerProxy, entries, &error);

    if (error) {
        ERROR ("Error calling org_sflphone_SFLphone_CallManager_set_history");
        g_error_free (error);
    }
}

void
dbus_confirm_sas (const callable_obj_t * c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_set_sa_sverified (callManagerProxy,
            c->_callID, &error);

    if (error) {
        ERROR ("Failed to call setSASVerified() on CallManager: %s",
               error->message);
        g_error_free (error);
    }
}

void
dbus_reset_sas (const callable_obj_t * c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_reset_sa_sverified (callManagerProxy,
            c->_callID, &error);

    if (error) {
        ERROR ("Failed to call resetSASVerified on CallManager: %s",
               error->message);
        g_error_free (error);
    }
}

void
dbus_set_confirm_go_clear (const callable_obj_t * c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_set_confirm_go_clear (callManagerProxy,
            c->_callID, &error);

    if (error) {
        ERROR ("Failed to call set_confirm_go_clear on CallManager: %s",
               error->message);
        g_error_free (error);
    }
}

void
dbus_request_go_clear (const callable_obj_t * c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_request_go_clear (callManagerProxy,
            c->_callID, &error);

    if (error) {
        ERROR ("Failed to call request_go_clear on CallManager: %s",
               error->message);
        g_error_free (error);
    }
}

gchar**
dbus_get_supported_tls_method()
{
    GError *error = NULL;
    gchar** array = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_supported_tls_method (
        configurationManagerProxy, &array, &error);

    if (error != NULL) {
        ERROR ("Failed to call get_supported_tls_method() on ConfigurationManager: %s",
               error->message);
        g_error_free (error);
    }

    return array;
}

GHashTable*
dbus_get_tls_settings_default (void)
{
    GError *error = NULL;
    GHashTable *results = NULL;

    org_sflphone_SFLphone_ConfigurationManager_get_tls_settings_default (
        configurationManagerProxy, &results, &error);

    if (error != NULL) {
        ERROR ("Error calling org_sflphone_SFLphone_ConfigurationManager_get_tls_settings_default");
        g_error_free (error);
    }

    return results;
}

gchar *
dbus_get_address_from_interface_name (gchar* interface)
{
    GError *error = NULL;
    gchar * address;

    org_sflphone_SFLphone_ConfigurationManager_get_addr_from_interface_name (
        configurationManagerProxy, interface, &address, &error);

    if (error != NULL) {
        ERROR ("Error calling org_sflphone_SFLphone_ConfigurationManager_get_addr_from_interface_name\n");
        g_error_free (error);
    }

    return address;

}

gchar **
dbus_get_all_ip_interface (void)
{
    GError *error = NULL;
    gchar ** array;

    if (!org_sflphone_SFLphone_ConfigurationManager_get_all_ip_interface (
                configurationManagerProxy, &array, &error)) {
        if (error->domain == DBUS_GERROR && error->code
                == DBUS_GERROR_REMOTE_EXCEPTION) {
            ERROR ("Caught remote method (get_all_ip_interface) exception  %s: %s", dbus_g_error_get_name (error), error->message);
        } else {
            ERROR ("Error while calling get_all_ip_interface: %s", error->message);
        }

        g_error_free (error);
        return NULL;
    } else {
        DEBUG ("DBus called get_all_ip_interface() on ConfigurationManager");
        return array;
    }
}

gchar **
dbus_get_all_ip_interface_by_name (void)
{
    GError *error = NULL;
    gchar ** array;

    if (!org_sflphone_SFLphone_ConfigurationManager_get_all_ip_interface_by_name (
                configurationManagerProxy, &array, &error)) {
        if (error->domain == DBUS_GERROR && error->code
                == DBUS_GERROR_REMOTE_EXCEPTION) {
            ERROR ("Caught remote method (get_all_ip_interface) exception  %s: %s", dbus_g_error_get_name (error), error->message);
        } else {
            ERROR ("Error while calling get_all_ip_interface: %s", error->message);
        }

        g_error_free (error);
        return NULL;
    } else {
        DEBUG ("DBus called get_all_ip_interface() on ConfigurationManager");
        return array;
    }
}

GHashTable*
dbus_get_shortcuts (void)
{
    GError *error = NULL;
    GHashTable * shortcuts;

    if (!org_sflphone_SFLphone_ConfigurationManager_get_shortcuts (
                configurationManagerProxy, &shortcuts, &error)) {
        if (error->domain == DBUS_GERROR && error->code
                == DBUS_GERROR_REMOTE_EXCEPTION) {
            ERROR ("Caught remote method (get_shortcuts) exception  %s: %s", dbus_g_error_get_name (error), error->message);
        } else {
            ERROR ("Error while calling get_shortcuts: %s", error->message);
        }

        g_error_free (error);
        return NULL;
    } else {
        return shortcuts;
    }
}

void
dbus_set_shortcuts (GHashTable * shortcuts)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_shortcuts (
        configurationManagerProxy, shortcuts, &error);


    if (error) {
        ERROR ("Failed to call set_shortcuts() on ConfigurationManager: %s",
               error->message);
        g_error_free (error);
    }
}

void
dbus_send_text_message (const gchar* callID, const gchar *message)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_send_text_message (
        callManagerProxy, callID, message, &error);

    if (error) {
        ERROR ("Failed to call send_text_message() on CallManager: %s",
               error->message);
        g_error_free (error);
    }
}
