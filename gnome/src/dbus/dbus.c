/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
 *  but WITHOUT ANY WARRANTY; without even the implied warranty f
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
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <glib/gi18n.h>
#include "str_utils.h"
#include "logger.h"
#include "calltab.h"
#include "callmanager-glue.h"
#include "configurationmanager-glue.h"
#ifdef SFL_VIDEO
#include "video_controls-glue.h"
#endif
#include "instance-glue.h"
#include "preferencesdialog.h"
#include "mainwindow.h"
#include "marshaller.h"
#include "sliders.h"
#include "statusicon.h"
#include "assistant.h"
#include "accountlist.h"
#include "accountlistconfigdialog.h"
#include "messaging/message_tab.h"

#include "dbus.h"
#include "actions.h"
#include "unused.h"

#ifdef SFL_VIDEO
#include "config/videoconf.h"
#include "video/video_callbacks.h"
#endif
#include "eel-gconf-extensions.h"
#include "account_schema.h"
#include "mainwindow.h"

#ifdef SFL_VIDEO
static DBusGProxy *video_proxy;
#endif
static DBusGProxy *call_proxy;
static DBusGProxy *config_proxy;
static DBusGProxy *instance_proxy;
// static DBusGProxy *session_manager_proxy;
static GDBusProxy *session_manager_proxy;

/* Returns TRUE if there was an error, FALSE otherwise */
static gboolean check_error(GError *error)
{
    if (error) {
        ERROR("%s", error->message);
        g_error_free(error);
        return TRUE;
    }
    return FALSE;
}

static void
new_call_created_cb(DBusGProxy *proxy UNUSED, const gchar *accountID,
                    const gchar *callID, const gchar *to, void *foo UNUSED)
{
    callable_obj_t *c = create_new_call(CALL, CALL_STATE_RINGING, callID,
                                        accountID, to, to);

    calllist_add_call(current_calls_tab, c);
    calltree_add_call(current_calls_tab, c, NULL);

    update_actions();
    calltree_display(current_calls_tab);
}

static void
incoming_call_cb(DBusGProxy *proxy UNUSED, const gchar *accountID,
                 const gchar *callID, const gchar *from, void *foo UNUSED)
{
    // We receive the from field under a formatted way. We want to extract the number and the name of the caller
    gchar *display_name = call_get_display_name(from);
    gchar *peer_number = call_get_peer_number(from);

    callable_obj_t *c = create_new_call(CALL, CALL_STATE_INCOMING, callID,
                                        accountID, display_name, peer_number);

    g_free(peer_number);
    g_free(display_name);

    status_tray_icon_blink(TRUE);
    popup_main_window();

    notify_incoming_call(c);
    sflphone_incoming_call(c);
}

static void
zrtp_negotiation_failed_cb(DBusGProxy *proxy UNUSED, const gchar *callID,
                           const gchar *reason, const gchar *severity,
                           void *foo UNUSED)
{
    main_window_zrtp_negotiation_failed(callID, reason, severity);
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c)
        notify_zrtp_negotiation_failed(c);
}

static void
volume_changed_cb(DBusGProxy *proxy UNUSED, const gchar *device, gdouble value,
                  void *foo UNUSED)
{
    set_slider_no_update(device, value);
}

static void
voice_mail_cb(DBusGProxy *proxy UNUSED, const gchar *accountID, guint nb,
              void *foo UNUSED)
{
    sflphone_notify_voice_mail(accountID, nb);
}

static void
incoming_message_cb(DBusGProxy *proxy UNUSED, const gchar *callID UNUSED,
                    const gchar *from UNUSED, const gchar *msg, void *foo UNUSED)
{
    // do not display message if instant messaging is disabled
    if (eel_gconf_key_exists(INSTANT_MESSAGING_ENABLED) &&
        !eel_gconf_get_integer(INSTANT_MESSAGING_ENABLED))
        return;

    callable_obj_t *call = calllist_get_call(current_calls_tab, callID);

    if (call) {
        new_text_message(call,msg);
    } else {
        conference_obj_t *conf = conferencelist_get(current_calls_tab, callID);
        if (!conf) {
            ERROR("Message received, but no recipient found");
            return;
        }

        new_text_message_conf(conf,msg,from);
    }
}

/**
 * Perform the right sflphone action based on the requested state
 */
static void
process_existing_call_state_change(callable_obj_t *c, const gchar *state)
{
    if (c == NULL) {
        ERROR("Pointer to call is NULL in %s\n", __func__);
        return;
    } else if (state == NULL) {
        ERROR("Pointer to state is NULL in %s\n", __func__);
        return;
    }

    if (g_strcmp0(state, "HUNGUP") == 0) {
        if (c->_state == CALL_STATE_CURRENT) {
            time(&c->_time_stop);
            calltree_update_call(history_tab, c);
        }

        calltree_update_call(history_tab, c);
        status_bar_display_account();
        sflphone_hung_up(c);
    } else if (g_strcmp0(state, "UNHOLD") == 0 || g_strcmp0(state, "CURRENT") == 0)
        sflphone_current(c);
    else if (g_strcmp0(state, "HOLD") == 0)
        sflphone_hold(c);
    else if (g_strcmp0(state, "RINGING") == 0)
        sflphone_ringing(c);
    else if (g_strcmp0(state, "FAILURE") == 0)
        sflphone_fail(c);
    else if (g_strcmp0(state, "BUSY") == 0)
        sflphone_busy(c);
}


/**
 * This function process call state changes in case the call have not been created yet.
 * This mainly occurs when another SFLphone client takes actions.
 */
static void
process_nonexisting_call_state_change(const gchar *callID, const gchar *state)
{
    if (callID == NULL) {
        ERROR("Pointer to call id is NULL in %s\n", __func__);
        return;
    } else if (state == NULL) {
        ERROR("Pointer to state is NULL in %s\n", __func__);
        return;
    } else if (g_strcmp0(state, "HUNGUP") == 0)
        return; // Could occur if a user picked up the phone and hung up without making a call

    // The callID is unknown, treat it like a new call
    // If it were an incoming call, we won't be here
    // It means that a new call has been initiated with an other client (cli for instance)
    if (g_strcmp0(state, "RINGING") == 0 || g_strcmp0(state, "CURRENT") == 0) {

        DEBUG("New ringing call! accountID: %s", callID);

        restore_call(callID);
        callable_obj_t *new_call = calllist_get_call(current_calls_tab, callID);
        if (new_call)
            calltree_add_call(current_calls_tab, new_call, NULL);
        update_actions();
        calltree_display(current_calls_tab);
    }
}

static void
call_state_cb(DBusGProxy *proxy UNUSED, const gchar *callID,
              const gchar *state, void *foo UNUSED)
{
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c)
        process_existing_call_state_change(c, state);
    else {
        WARN("Call does not exist");
        process_nonexisting_call_state_change(callID, state);
    }
}

static void
toggle_im(conference_obj_t *conf, gboolean activate UNUSED)
{
    for (GSList *p = conf->participant_list; p; p = g_slist_next(p)) {
        //callable_obj_t *call = calllist_get_call(current_calls_tab, p->data);

        /*TODO elepage(2012) Implement IM messaging toggle here*/
    }
}

static void
conference_changed_cb(DBusGProxy *proxy UNUSED, const gchar *confID,
                      const gchar *state, void *foo UNUSED)
{
    DEBUG("Conference state changed: %s\n", state);

    conference_obj_t* changed_conf = conferencelist_get(current_calls_tab, confID);
    if (changed_conf == NULL) {
        ERROR("Conference is NULL in conference state changed");
        return;
    }

    // remove old conference from calltree
    calltree_remove_conference(current_calls_tab, changed_conf);

    // update conference state
    if (g_strcmp0(state, "ACTIVE_ATTACHED") == 0)
        changed_conf->_state = CONFERENCE_STATE_ACTIVE_ATTACHED;
    else if (g_strcmp0(state, "ACTIVE_DETACHED") == 0)
        changed_conf->_state = CONFERENCE_STATE_ACTIVE_DETACHED;
    else if (g_strcmp0(state, "ACTIVE_ATTACHED_REC") == 0)
        changed_conf->_state = CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD;
    else if (g_strcmp0(state, "ACTIVE_DETACHED_REC") == 0)
        changed_conf->_state = CONFERENCE_STATE_ACTIVE_DETACHED_RECORD;
    else if (g_strcmp0(state, "HOLD") == 0)
        changed_conf->_state = CONFERENCE_STATE_HOLD;
    else if (g_strcmp0(state, "HOLD_REC") == 0)
        changed_conf->_state = CONFERENCE_STATE_HOLD_RECORD;
    else
        DEBUG("Error: conference state not recognized");

    // reactivate instant messaging window for these calls
    toggle_im(changed_conf, TRUE);

    gchar **list = dbus_get_participant_list(changed_conf->_confID);
    conference_participant_list_update(list, changed_conf);
    g_strfreev(list);

    // deactivate instant messaging window for new participants
    toggle_im(changed_conf, FALSE);
    calltree_add_conference_to_current_calls(changed_conf);
}

static void
conference_created_cb(DBusGProxy *proxy UNUSED, const gchar *confID, void *foo UNUSED)
{
    DEBUG("Conference %s added", confID);

    conference_obj_t *new_conf = create_new_conference(CONFERENCE_STATE_ACTIVE_ATTACHED, confID);

    gchar **participants = dbus_get_participant_list(new_conf->_confID);

    // Update conference list
    conference_participant_list_update(participants, new_conf);

    // Add conference ID in in each calls
    for (gchar **part = participants; part && *part; ++part) {
        callable_obj_t *call = calllist_get_call(current_calls_tab, *part);

        /*TODO elepage (2012) implement merging IM conversation here*/

        // if one of these participants is currently recording, the whole conference will be recorded
        if (dbus_get_is_recording(call))
            new_conf->_state = CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD;

        call->_historyConfID = g_strdup(confID);
    }

    g_strfreev(participants);

    time(&new_conf->_time_start);

    conferencelist_add(current_calls_tab, new_conf);
    calltree_add_conference_to_current_calls(new_conf);
}

static void
conference_removed_cb(DBusGProxy *proxy UNUSED, const gchar *confID,
                      void *foo UNUSED)
{
    DEBUG("Conference removed %s", confID);
    conference_obj_t *c = conferencelist_get(current_calls_tab, confID);
    if(c == NULL) {
        ERROR("Could not find conference %s from list", confID);
        return;
    }

    calltree_remove_conference(current_calls_tab, c);

    /*TODO elepage(2012) implement unmerging of IM here*/

    // remove all participants for this conference
    for (GSList *p = c->participant_list; p; p = g_slist_next(p)) {
        //callable_obj_t *call = calllist_get_call(current_calls_tab, p->data);
        /*TODO elepage(2012) implement unmerging of IM here*/
    }

    conferencelist_remove(current_calls_tab, c->_confID);
}

static void
record_playback_filepath_cb(DBusGProxy *proxy UNUSED, const gchar *id,
                            const gchar *filepath)
{
    DEBUG("Filepath for %s: %s", id, filepath);
    callable_obj_t *call = calllist_get_call(current_calls_tab, id);
    conference_obj_t *conf = conferencelist_get(current_calls_tab, id);

    if (call && conf) {
        ERROR("Two objects for this callid");
        return;
    }

    if (!call && !conf) {
        ERROR("Could not get object");
        return;
    }

    if (call && call->_recordfile == NULL)
        call->_recordfile = g_strdup(filepath);
    else if (conf && conf->_recordfile == NULL)
        conf->_recordfile = g_strdup(filepath);
}

static void
record_playback_stopped_cb(DBusGProxy *proxy UNUSED, const gchar *filepath)
{
    DEBUG("Playback stopped for %s", filepath);
    const gint calllist_size = calllist_get_size(history_tab);

    for (gint i = 0; i < calllist_size; i++) {
        callable_obj_t *call = calllist_get_nth(history_tab, i);

        if (call == NULL) {
            ERROR("Could not find %dth call", i);
            break;
        }
        if (g_strcmp0(call->_recordfile, filepath) == 0)
            call->_record_is_playing = FALSE;
    }

    update_actions();
}

static void
update_playback_scale_cb(DBusGProxy *proxy UNUSED, guint position, guint size)
{
    main_window_update_playback_scale(position, size);
}

static void
registration_state_changed_cb(DBusGProxy *proxy UNUSED, const gchar *accountID,
                              guint state, void *foo UNUSED)
{
    DEBUG("DBus: Registration state changed to %s for account %s",
          account_state_name(state), accountID);
    account_t *acc = account_list_get_by_id(accountID);
    if (acc) {
        acc->state = state;
        update_account_list_status_bar(acc);
    }
}

static void
accounts_changed_cb(DBusGProxy *proxy UNUSED, void *foo UNUSED)
{
    sflphone_fill_account_list();
    sflphone_fill_ip2ip_profile();
    status_bar_display_account();
    statusicon_set_tooltip();
}

static void
stun_status_failure_cb(DBusGProxy *proxy UNUSED, const gchar *accountID, void *foo UNUSED)
{
    ERROR("Error: Stun status failure: account %s failed to setup STUN",
          accountID);
    // Disable STUN for the account that tried to create the STUN transport
    account_t *account = account_list_get_by_id(accountID);
    if (account) {
        account_replace(account, CONFIG_STUN_ENABLE, "false");
        dbus_set_account_details(account);
    }
}

static void
stun_status_success_cb(DBusGProxy *proxy UNUSED, const gchar *message UNUSED, void *foo UNUSED)
{
    DEBUG("STUN setup successful");
}

static void
transfer_succeeded_cb(DBusGProxy *proxy UNUSED, void *foo UNUSED)
{
    sflphone_display_transfer_status("Transfer successful");
}

static void
transfer_failed_cb(DBusGProxy *proxy UNUSED, void *foo UNUSED)
{
    sflphone_display_transfer_status("Transfer failed");
}

static void
secure_sdes_on_cb(DBusGProxy *proxy UNUSED, const gchar *callID, void *foo UNUSED)
{
    DEBUG("SRTP using SDES is on");
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c) {
        sflphone_srtp_sdes_on(c);
        notify_secure_on(c);
    }
}

static void
secure_sdes_off_cb(DBusGProxy *proxy UNUSED, const gchar *callID, void *foo UNUSED)
{
    DEBUG("SRTP using SDES is off");
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c) {
        sflphone_srtp_sdes_off(c);
        notify_secure_off(c);
    }
}

static void
secure_zrtp_on_cb(DBusGProxy *proxy UNUSED, const gchar *callID,
                  const gchar *cipher, void *foo UNUSED)
{
    DEBUG("SRTP using ZRTP is ON secure_on_cb");
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c) {
        c->_srtp_cipher = g_strdup(cipher);
        sflphone_srtp_zrtp_on(c);
        notify_secure_on(c);
    }
}

static void
secure_zrtp_off_cb(DBusGProxy *proxy UNUSED, const gchar *callID, void *foo UNUSED)
{
    DEBUG("SRTP using ZRTP is OFF");
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c) {
        sflphone_srtp_zrtp_off(c);
        notify_secure_off(c);
    }
}

static void
show_zrtp_sas_cb(DBusGProxy *proxy UNUSED, const gchar *callID, const gchar *sas,
                 gboolean verified, void *foo UNUSED)
{
    DEBUG("Showing SAS");
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c)
        sflphone_srtp_zrtp_show_sas(c, sas, verified);
}

static void
confirm_go_clear_cb(DBusGProxy *proxy UNUSED, const gchar *callID, void *foo UNUSED)
{
    DEBUG("Confirm Go Clear request");
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c)
        main_window_confirm_go_clear(c);
}

static void
zrtp_not_supported_cb(DBusGProxy *proxy UNUSED, const gchar *callID, void *foo UNUSED)
{
    DEBUG("ZRTP not supported on the other end");
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c) {
        main_window_zrtp_not_supported(c);
        notify_zrtp_not_supported(c);
    }
}

static void
sip_call_state_cb(DBusGProxy *proxy UNUSED, const gchar *callID,
                  const gchar *description, guint code, void *foo UNUSED)
{
    DEBUG("Sip call state changed %s", callID);
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c)
        sflphone_call_state_changed(c, description, code);
}

static void
error_alert(DBusGProxy *proxy UNUSED, int err, void *foo UNUSED)
{
    const gchar *msg;

    switch (err) {
        case ALSA_PLAYBACK_DEVICE:
            msg = _("ALSA notification: Error while opening playback device");
            break;
        case ALSA_CAPTURE_DEVICE:
            msg = _("ALSA notification: Error while opening capture device");
            break;
        case PULSEAUDIO_NOT_RUNNING:
            msg = _("Pulseaudio notification: Pulseaudio is not running");
            break;
        case CODECS_NOT_LOADED:
            msg = _("Codecs notification: Codecs not found");
            break;
        default:
            return;
    }

    ERROR("%s", msg);
}

static void
screensaver_dbus_proxy_new_cb (GObject * source UNUSED, GAsyncResult *result, gpointer user_data UNUSED)
{
    DEBUG("Session manager connection callback");

    session_manager_proxy = g_dbus_proxy_new_for_bus_finish (result, NULL);
    if (session_manager_proxy == NULL)
        ERROR("could not initialize gnome session manager");
}

#define GS_SERVICE   "org.gnome.SessionManager"
#define GS_PATH      "/org/gnome/SessionManager"
#define GS_INTERFACE "org.gnome.SessionManager"

gboolean dbus_connect_session_manager(DBusGConnection *connection)
{

    if (connection == NULL) {
        ERROR("connection is NULL");
        return FALSE;
    }
/*
    session_manager_proxy = dbus_g_proxy_new_for_name(connection,
                            "org.gnome.SessionManager", "/org/gnome/SessionManager/Inhibitor",
                            "org.gnome.SessionManager.Inhibitor");

    if(session_manager_proxy == NULL) {
        ERROR("Error, could not create session manager proxy");
        return FALSE;
    }
*/

    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION,
                             G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                             NULL, GS_SERVICE, GS_PATH, GS_INTERFACE, NULL,
                             screensaver_dbus_proxy_new_cb, NULL);

    DEBUG("Connected to gnome session manager");

    return TRUE;
}

gboolean dbus_connect(GError **error)
{
    const char *dbus_message_bus_name = "org.sflphone.SFLphone";
    const char *dbus_object_instance = "/org/sflphone/SFLphone/Instance";
    const char *dbus_interface = "org.sflphone.SFLphone.Instance";
    const char *callmanager_object_instance = "/org/sflphone/SFLphone/CallManager";
    const char *callmanager_interface = "org.sflphone.SFLphone.CallManager";
    const char *configurationmanager_object_instance = "/org/sflphone/SFLphone/ConfigurationManager";
    const char *configurationmanager_interface = "org.sflphone.SFLphone.ConfigurationManager";

    g_type_init();

    DBusGConnection *connection = dbus_g_bus_get(DBUS_BUS_SESSION, error);
    if (connection == NULL) {
        ERROR("could not establish connection with session bus");
        return FALSE;
    }

    /* Create a proxy object for the "bus driver" (name "org.freedesktop.DBus") */
    DEBUG("Connect to message bus:     %s", dbus_message_bus_name);
    DEBUG("           object instance: %s", dbus_object_instance);
    DEBUG("           dbus interface:  %s", dbus_interface);

    instance_proxy = dbus_g_proxy_new_for_name(connection, dbus_message_bus_name, dbus_object_instance, dbus_interface);
    if (instance_proxy == NULL) {
        ERROR("Error: Failed to connect to %s", dbus_message_bus_name);
        return FALSE;
    }

    DEBUG("Connect to object instance: %s", callmanager_object_instance);
    DEBUG("           dbus interface:  %s", callmanager_interface);

    call_proxy = dbus_g_proxy_new_for_name(connection, dbus_message_bus_name, callmanager_object_instance, callmanager_interface);
    if (call_proxy == NULL) {
        ERROR("Error: Failed to connect to %s", callmanager_object_instance);
        return FALSE;
    }

    config_proxy = dbus_g_proxy_new_for_name(connection, dbus_message_bus_name, configurationmanager_object_instance, configurationmanager_interface);
    if (config_proxy == NULL) {
        ERROR("Error: Failed to connect to %s", configurationmanager_object_instance);
        return FALSE;
    }

    /* Register INT Marshaller */
    dbus_g_object_register_marshaller(g_cclosure_user_marshal_VOID__INT,
                                      G_TYPE_NONE, G_TYPE_INT, G_TYPE_INVALID);

    /* Register INT INT Marshaller */
    dbus_g_object_register_marshaller(g_cclosure_user_marshal_VOID__INT_INT,
                                      G_TYPE_NONE, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID);

    /* Register STRING Marshaller */
    dbus_g_object_register_marshaller(g_cclosure_user_marshal_VOID__STRING,
                                      G_TYPE_NONE, G_TYPE_STRING, G_TYPE_INVALID);

    /* Register STRING INT Marshaller */
    dbus_g_object_register_marshaller(g_cclosure_user_marshal_VOID__STRING_INT,
                                      G_TYPE_NONE, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INVALID);

    /* Register STRING DOUBLE Marshaller */
    dbus_g_object_register_marshaller(
        g_cclosure_user_marshal_VOID__STRING_DOUBLE, G_TYPE_NONE, G_TYPE_STRING,
        G_TYPE_DOUBLE, G_TYPE_INVALID);

    /* Register STRING STRING Marshaller */
    dbus_g_object_register_marshaller(
        g_cclosure_user_marshal_VOID__STRING_STRING, G_TYPE_NONE, G_TYPE_STRING,
        G_TYPE_STRING, G_TYPE_INVALID);

    /* Register STRING INT INT Marshaller */
    dbus_g_object_register_marshaller(
            g_cclosure_user_marshal_VOID__STRING_INT_INT, G_TYPE_NONE,
            G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID);

    /* Register STRING STRING BOOL Marshaller */
    dbus_g_object_register_marshaller(
        g_cclosure_user_marshal_VOID__STRING_STRING_BOOL, G_TYPE_NONE,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_INVALID);

    /* Register STRING STRING INT Marshaller */
    dbus_g_object_register_marshaller(
        g_cclosure_user_marshal_VOID__STRING_STRING_INT, G_TYPE_NONE,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INVALID);

    /* Register STRING STRING STRING Marshaller */
    dbus_g_object_register_marshaller(
        g_cclosure_user_marshal_VOID__STRING_STRING_STRING, G_TYPE_NONE,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);

    /* Register STRING STRING INT INT Marshaller */
    dbus_g_object_register_marshaller(
        g_cclosure_user_marshal_VOID__STRING_STRING_INT_INT, G_TYPE_NONE,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID);

    DEBUG("Adding callmanager Dbus signals");

    /* Incoming call */
    dbus_g_proxy_add_signal(call_proxy, "newCallCreated", G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "newCallCreated",
                                G_CALLBACK(new_call_created_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "incomingCall", G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "incomingCall",
                                G_CALLBACK(incoming_call_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "zrtpNegotiationFailed",
                            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "zrtpNegotiationFailed",
                                G_CALLBACK(zrtp_negotiation_failed_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "callStateChanged", G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "callStateChanged",
                                G_CALLBACK(call_state_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "voiceMailNotify", G_TYPE_STRING,
                            G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "voiceMailNotify",
                                G_CALLBACK(voice_mail_cb), NULL, NULL);

    dbus_g_proxy_add_signal(config_proxy, "registrationStateChanged", G_TYPE_STRING,
                            G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(config_proxy, "registrationStateChanged",
                                G_CALLBACK(registration_state_changed_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "incomingMessage", G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "incomingMessage",
                                G_CALLBACK(incoming_message_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "volumeChanged", G_TYPE_STRING,
                            G_TYPE_DOUBLE, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "volumeChanged",
                                G_CALLBACK(volume_changed_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "transferSucceeded", G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "transferSucceeded",
                                G_CALLBACK(transfer_succeeded_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "transferFailed", G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "transferFailed",
                                G_CALLBACK(transfer_failed_cb), NULL, NULL);

    /* Conference related callback */
    dbus_g_proxy_add_signal(call_proxy, "conferenceChanged", G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "conferenceChanged",
                                G_CALLBACK(conference_changed_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "conferenceCreated", G_TYPE_STRING,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "conferenceCreated",
                                G_CALLBACK(conference_created_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "conferenceRemoved", G_TYPE_STRING,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "conferenceRemoved",
                                G_CALLBACK(conference_removed_cb), NULL, NULL);

    /* Playback related signals */
    dbus_g_proxy_add_signal(call_proxy, "recordPlaybackFilepath", G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "recordPlaybackFilepath",
                                G_CALLBACK(record_playback_filepath_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "recordPlaybackStopped", G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "recordPlaybackStopped",
                                G_CALLBACK(record_playback_stopped_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "updatePlaybackScale", G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "updatePlaybackScale",
                                G_CALLBACK(update_playback_scale_cb), NULL, NULL);

    /* Security related callbacks */
    dbus_g_proxy_add_signal(call_proxy, "secureSdesOn", G_TYPE_STRING,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "secureSdesOn",
                                G_CALLBACK(secure_sdes_on_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "secureSdesOff", G_TYPE_STRING,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "secureSdesOff",
                                G_CALLBACK(secure_sdes_off_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "showSAS", G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "showSAS",
                                G_CALLBACK(show_zrtp_sas_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "secureZrtpOn", G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "secureZrtpOn",
                                G_CALLBACK(secure_zrtp_on_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "secureZrtpOff", G_TYPE_STRING,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "secureZrtpOff",
                                G_CALLBACK(secure_zrtp_off_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "zrtpNotSuppOther", G_TYPE_STRING,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "zrtpNotSuppOther",
                                G_CALLBACK(zrtp_not_supported_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "confirmGoClear", G_TYPE_STRING,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "confirmGoClear",
                                G_CALLBACK(confirm_go_clear_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "sipCallStateChanged",
                            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "sipCallStateChanged",
                                G_CALLBACK(sip_call_state_cb), NULL, NULL);


    DEBUG("Adding configurationmanager Dbus signals");

    dbus_g_proxy_add_signal(config_proxy, "accountsChanged", G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(config_proxy, "accountsChanged",
                                G_CALLBACK(accounts_changed_cb), NULL, NULL);

    dbus_g_proxy_add_signal(config_proxy, "stunStatusFailure", G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(config_proxy, "stunStatusFailure",
                                G_CALLBACK(stun_status_failure_cb), NULL, NULL);

    dbus_g_proxy_add_signal(config_proxy, "stunStatusSuccess", G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(config_proxy, "stunStatusSuccess",
                                G_CALLBACK(stun_status_success_cb), NULL, NULL);

    dbus_g_proxy_add_signal(config_proxy, "errorAlert", G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(config_proxy, "errorAlert",
                                G_CALLBACK(error_alert), NULL, NULL);

#ifdef SFL_VIDEO
    const gchar *videocontrols_object_instance = "/org/sflphone/SFLphone/VideoControls";
    const gchar *videocontrols_interface = "org.sflphone.SFLphone.VideoControls";
    video_proxy = dbus_g_proxy_new_for_name(connection, dbus_message_bus_name,
            videocontrols_object_instance, videocontrols_interface);
    if (video_proxy == NULL) {
        ERROR("Error: Failed to connect to %s", videocontrols_object_instance);
        return FALSE;
    }
    /* Video related signals */
    dbus_g_proxy_add_signal(video_proxy, "deviceEvent", G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(video_proxy, "deviceEvent",
            G_CALLBACK(video_device_event_cb), NULL, NULL);

    dbus_g_proxy_add_signal(video_proxy, "startedDecoding", G_TYPE_STRING,
            G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(video_proxy, "startedDecoding",
            G_CALLBACK(started_decoding_video_cb), NULL,
            NULL);

    dbus_g_proxy_add_signal(video_proxy, "stoppedDecoding",
            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(video_proxy, "stoppedDecoding",
            G_CALLBACK(stopped_decoding_video_cb),
            NULL, NULL);
#endif

    /* Defines a default timeout for the proxies */
#if HAVE_DBUS_G_PROXY_SET_DEFAULT_TIMEOUT
    static const int DEFAULT_DBUS_TIMEOUT = 30000;
    dbus_g_proxy_set_default_timeout(call_proxy, DEFAULT_DBUS_TIMEOUT);
    dbus_g_proxy_set_default_timeout(instance_proxy, DEFAULT_DBUS_TIMEOUT);
    dbus_g_proxy_set_default_timeout(config_proxy, DEFAULT_DBUS_TIMEOUT);
#ifdef SFL_VIDEO
    dbus_g_proxy_set_default_timeout(video_proxy, DEFAULT_DBUS_TIMEOUT);
#endif
#endif

    gboolean status = dbus_connect_session_manager(connection);
    if(status == FALSE) {
        ERROR("could not connect to gnome session manager");
        return FALSE;
    }

    return TRUE;
}

void dbus_clean()
{
#ifdef SFL_VIDEO
        g_object_unref(video_proxy);
#endif
    g_object_unref(call_proxy);
    g_object_unref(config_proxy);
    g_object_unref(instance_proxy);
}

void dbus_hold(const callable_obj_t *c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_hold(call_proxy, c->_callID, &error);
    check_error(error);
}

void
dbus_unhold(const callable_obj_t *c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_unhold(call_proxy, c->_callID, &error);
    check_error(error);
}

void
dbus_hold_conference(const conference_obj_t *c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_hold_conference(call_proxy, c->_confID, &error);
    check_error(error);
}

void
dbus_unhold_conference(const conference_obj_t *c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_unhold_conference(call_proxy, c->_confID, &error);
    check_error(error);
}

gboolean
dbus_start_recorded_file_playback(const gchar *filepath)
{
    GError *error = NULL;
    gboolean result;

    org_sflphone_SFLphone_CallManager_start_recorded_file_playback(call_proxy, filepath, &result, &error);
    check_error(error);
    return result;
}

void
dbus_stop_recorded_file_playback(const gchar *filepath)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_stop_recorded_file_playback(call_proxy, filepath, &error);
    check_error(error);
}

static void
hang_up_reply_cb(DBusGProxy *proxy UNUSED, GError *error, gpointer userdata UNUSED)
{
    check_error(error);
}

void
dbus_hang_up(const callable_obj_t *c)
{
    org_sflphone_SFLphone_CallManager_hang_up_async(call_proxy, c->_callID, hang_up_reply_cb, NULL);
}

void
dbus_hang_up_conference(const conference_obj_t *c)
{
    org_sflphone_SFLphone_CallManager_hang_up_conference_async(call_proxy, c->_confID, hang_up_reply_cb, NULL);
}

void
dbus_transfer(const callable_obj_t *c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_transfer(call_proxy, c->_callID, c->_trsft_to, &error);
    check_error(error);
}

void
dbus_attended_transfer(const callable_obj_t *transfer, const callable_obj_t *target)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_attended_transfer(call_proxy, transfer->_callID,
                           target->_callID, &error);
    check_error(error);
}

void
dbus_accept(const callable_obj_t *c)
{
    status_tray_icon_blink(FALSE);
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_accept(call_proxy, c->_callID, &error);
    check_error(error);
}

void
dbus_refuse(const callable_obj_t *c)
{
    status_tray_icon_blink(FALSE);
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_refuse(call_proxy, c->_callID, &error);
    check_error(error);
}

void
dbus_place_call(const callable_obj_t *c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_place_call(call_proxy, c->_accountID, c->_callID, c->_peer_number,
                    &error);
    check_error(error);
}

gchar **
dbus_account_list()
{
    GError *error = NULL;
    char **array = NULL;

    org_sflphone_SFLphone_ConfigurationManager_get_account_list(config_proxy, &array, &error);
    check_error(error);

    return array;
}

GHashTable *
dbus_get_account_details(const gchar *accountID)
{
    GError *error = NULL;
    GHashTable *details = NULL;

    org_sflphone_SFLphone_ConfigurationManager_get_account_details(config_proxy, accountID, &details, &error);
    check_error(error);

    return details;
}

void
dbus_set_credentials(account_t *a)
{
    g_assert(a);
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_credentials(config_proxy, a->accountID,
                           a->credential_information, &error);
    check_error(error);
}

void
dbus_get_credentials(account_t *a)
{
    g_assert(a);
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_credentials(config_proxy,
            a->accountID, &a->credential_information, &error);
    check_error(error);
}

GHashTable *
dbus_get_ip2_ip_details(void)
{
    GError *error = NULL;
    GHashTable *details = NULL;

    org_sflphone_SFLphone_ConfigurationManager_get_ip2_ip_details(config_proxy, &details, &error);
    check_error(error);

    return details;
}

void
dbus_send_register(const gchar *accountID, gboolean enable)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_send_register(config_proxy, accountID, enable, &error);
    check_error(error);
}

void
dbus_remove_account(const gchar *accountID)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_remove_account(config_proxy, accountID, &error);
    account_list_remove(accountID);
    check_error(error);
}

void
dbus_set_account_details(const account_t *a)
{
    g_assert(a);
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_account_details(config_proxy, a->accountID, a->properties,
                               &error);
    check_error(error);
}

void
dbus_add_account(account_t *a)
{
    g_assert(a);
    g_assert(a->accountID);
    g_assert(a->properties);
    g_free(a->accountID);
    GError *error = NULL;
    a->accountID = NULL;
    org_sflphone_SFLphone_ConfigurationManager_add_account(config_proxy, a->properties, &a->accountID,
                       &error);
    check_error(error);
}

void
dbus_set_volume(const gchar *device, gdouble value)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_set_volume(call_proxy, device, value, &error);
    check_error(error);
}

gdouble
dbus_get_volume(const gchar *device)
{
    gdouble value;
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_get_volume(call_proxy, device, &value, &error);
    check_error(error);
    return value;
}

void
dbus_play_dtmf(const gchar *key)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_play_dt_mf(call_proxy, key, &error);
    check_error(error);
}

void
dbus_start_tone(int start, guint type)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_start_tone(call_proxy, start, type, &error);
    check_error(error);
}

gboolean
dbus_register(int pid, const gchar *name, GError **error)
{
    return org_sflphone_SFLphone_Instance_register(instance_proxy, pid, name,
                                                   error);
}

void
dbus_unregister(int pid)
{
    GError *error = NULL;
    org_sflphone_SFLphone_Instance_unregister_async(instance_proxy, pid, NULL, NULL);
    check_error(error);
}

GArray *
dbus_audio_codec_list()
{
    GError *error = NULL;
    GArray *array = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_audio_codec_list(config_proxy, &array, &error);
    check_error(error);
    return array;
}

#ifdef SFL_VIDEO
GPtrArray *
dbus_get_video_codecs(const gchar *accountID)
{
    GError *error = NULL;
    GPtrArray *array = NULL;
    org_sflphone_SFLphone_VideoControls_get_codecs(video_proxy, accountID, &array, &error);
    check_error(error);
    return array;
}

void
dbus_set_video_codecs(const gchar *accountID, const GPtrArray *list)
{
    GError *error = NULL;
    org_sflphone_SFLphone_VideoControls_set_codecs(video_proxy, accountID, list, &error);
    check_error(error);
}
#endif

gchar **
dbus_audio_codec_details(int payload)
{
    GError *error = NULL;
    gchar **array;
    org_sflphone_SFLphone_ConfigurationManager_get_audio_codec_details(config_proxy, payload, &array, &error);
    check_error(error);
    return array;
}

#ifdef SFL_VIDEO

gchar *
dbus_get_current_video_codec_name(const callable_obj_t *c)
{
    gchar *codecName = NULL;
    GError *error = NULL;

    org_sflphone_SFLphone_VideoControls_get_current_codec_name(video_proxy,
            c->_callID, &codecName, &error);

    if (check_error(error)) {
        g_free(codecName);
        codecName = g_strdup("");
    }

    return codecName;
}
#endif

gchar *
dbus_get_current_audio_codec_name(const callable_obj_t *c)
{
    gchar *codecName = NULL;
    GError *error = NULL;

    org_sflphone_SFLphone_CallManager_get_current_audio_codec_name(call_proxy, c->_callID, &codecName,
                                      &error);
    check_error(error);
    return codecName;
}

GArray *
dbus_get_active_audio_codec_list(const gchar *accountID)
{
    GArray *array = NULL;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_active_audio_codec_list(config_proxy, accountID, &array,
                                       &error);
    check_error(error);

    return array;
}

void
dbus_set_active_audio_codec_list(const gchar **list, const gchar *accountID)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_active_audio_codec_list(config_proxy, list, accountID, &error);
    check_error(error);
}

/**
 * Get a list of output supported audio plugins
 */
gchar **
dbus_get_audio_plugin_list()
{
    gchar **array = NULL;
    GError *error = NULL;

    org_sflphone_SFLphone_ConfigurationManager_get_audio_plugin_list(config_proxy, &array, &error);
    check_error(error);

    return array;
}

void
dbus_set_audio_plugin(const gchar *audioPlugin)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_audio_plugin(config_proxy, audioPlugin, &error);
    check_error(error);
}

/**
 * Get all output devices index supported by current audio manager
 */
gchar **
dbus_get_audio_output_device_list()
{
    gchar **array = NULL;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_audio_output_device_list(config_proxy, &array, &error);
    check_error(error);
    return array;
}

/**
 * Set audio output device from its index
 */
void
dbus_set_audio_output_device(int device)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_audio_output_device(config_proxy, device, &error);
    check_error(error);
}

/**
 * Set audio input device from its index
 */
void
dbus_set_audio_input_device(int device)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_audio_input_device(config_proxy, device, &error);
    check_error(error);
}

/**
 * Set adio ringtone device from its index
 */
void
dbus_set_audio_ringtone_device(int device)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_audio_ringtone_device(config_proxy, device, &error);
    check_error(error);
}

/**
 * Get all input devices index supported by current audio manager
 */
gchar **
dbus_get_audio_input_device_list()
{
    gchar **array = NULL;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_audio_input_device_list(config_proxy, &array, &error);
    check_error(error);

    return array;
}

/**
 * Get output device index and input device index
 */
gchar **
dbus_get_current_audio_devices_index()
{
    gchar **array = NULL;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_current_audio_devices_index(config_proxy, &array, &error);
    check_error(error);

    return array;
}

/**
 * Get index
 */
int
dbus_get_audio_device_index(const gchar *name)
{
    int device_index = 0;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_audio_device_index(config_proxy, name, &device_index, &error);
    check_error(error);

    return device_index;
}

/**
 * Get audio plugin
 */
gchar *
dbus_get_current_audio_output_plugin()
{
    gchar *plugin;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_current_audio_output_plugin(config_proxy, &plugin, &error);
    if (check_error(error))
        plugin = g_strdup("");

    return plugin;
}


/**
 * Get noise reduction state
 */
gchar *
dbus_get_noise_suppress_state()
{
    gchar *state;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_noise_suppress_state(config_proxy, &state, &error);

    if (check_error(error))
        state = g_strdup("");

    return state;
}

/**
 * Set noise reduction state
 */
void
dbus_set_noise_suppress_state(const gchar *state)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_noise_suppress_state(config_proxy, state, &error);

    if (error) {
        ERROR("Failed to call set_noise_suppress_state() on "
              "ConfigurationManager: %s", error->message);
        g_error_free(error);
    }
}

gchar *
dbus_get_echo_cancel_state(void)
{
    GError *error = NULL;
    gchar *state;
    org_sflphone_SFLphone_ConfigurationManager_get_echo_cancel_state(config_proxy, &state, &error);

    if (check_error(error))
        state = g_strdup("");

    return state;
}

void
dbus_set_echo_cancel_state(const gchar *state)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_echo_cancel_state(config_proxy, state, &error);
    check_error(error);
}

int
dbus_get_echo_cancel_tail_length(void)
{
    GError *error = NULL;
    int length = 0;
    org_sflphone_SFLphone_ConfigurationManager_get_echo_cancel_tail_length(config_proxy, &length, &error);
    check_error(error);
    return length;
}

void
dbus_set_echo_cancel_tail_length(int length)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_echo_cancel_tail_length(config_proxy, length, &error);
    check_error(error);
}

int
dbus_get_echo_cancel_delay(void)
{
    GError *error = NULL;
    int delay = 0;
    org_sflphone_SFLphone_ConfigurationManager_get_echo_cancel_delay(config_proxy, &delay, &error);
    check_error(error);

    return delay;
}

void
dbus_set_echo_cancel_delay(int delay)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_echo_cancel_delay(config_proxy, delay, &error);
    check_error(error);
}


int
dbus_is_iax2_enabled()
{
    int res = 0;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_is_iax2_enabled(config_proxy, &res, &error);
    check_error(error);

    return res;
}

void
dbus_join_participant(const gchar *sel_callID, const gchar *drag_callID)
{
    DEBUG("Join participant %s and %s\n", sel_callID, drag_callID);
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_join_participant(call_proxy, sel_callID, drag_callID, &error);
    check_error(error);
}

void
dbus_create_conf_from_participant_list(const gchar **list)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_create_conf_from_participant_list(call_proxy, list, &error);
    check_error(error);
}

void
dbus_add_participant(const gchar *callID, const gchar *confID)
{
    DEBUG("Add participant %s to %s\n", callID, confID);
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_add_participant(call_proxy, callID, confID, &error);
    check_error(error);
}

void
dbus_add_main_participant(const gchar *confID)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_add_main_participant(call_proxy, confID, &error);
    check_error(error);
}

void
dbus_detach_participant(const gchar *callID)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_detach_participant(call_proxy, callID, &error);
    check_error(error);
}

void
dbus_join_conference(const gchar *sel_confID, const gchar *drag_confID)
{
    DEBUG("dbus_join_conference %s and %s\n", sel_confID, drag_confID);
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_join_conference(call_proxy, sel_confID, drag_confID, &error);
    check_error(error);
}

void
dbus_set_record(const gchar *id)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_set_recording(call_proxy, id, &error);
    check_error(error);
}

gboolean
dbus_get_is_recording(const callable_obj_t *c)
{
    GError *error = NULL;
    gboolean isRecording;
    org_sflphone_SFLphone_CallManager_get_is_recording(call_proxy, c->_callID, &isRecording, &error);
    check_error(error);

    return isRecording;
}

void
dbus_set_record_path(const gchar *path)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_record_path(config_proxy, path, &error);
    check_error(error);
}

gchar *
dbus_get_record_path(void)
{
    GError *error = NULL;
    gchar *path;
    org_sflphone_SFLphone_ConfigurationManager_get_record_path(config_proxy, &path, &error);
    check_error(error);

    return path;
}

void dbus_set_record_playback_seek(gdouble value) {
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_record_playback_seek(call_proxy, value, &error);
    check_error(error);
}

void dbus_set_is_always_recording(const gboolean alwaysRec)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_is_always_recording(config_proxy, alwaysRec, &error);
    check_error(error);
}

gboolean dbus_get_is_always_recording(void)
{
    GError *error = NULL;
    gboolean alwaysRec;
    org_sflphone_SFLphone_ConfigurationManager_get_is_always_recording(config_proxy, &alwaysRec, &error);
    check_error(error);

    return alwaysRec;
}

void
dbus_set_history_limit(guint days)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_history_limit(config_proxy, days, &error);
    check_error(error);
}

guint
dbus_get_history_limit(void)
{
    GError *error = NULL;
    gint days = 30;
    org_sflphone_SFLphone_ConfigurationManager_get_history_limit(config_proxy, &days, &error);
    check_error(error);

    return days;
}

void
dbus_clear_history(void)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_clear_history(config_proxy, &error);
    check_error(error);
}

void
dbus_set_audio_manager(const gchar *api)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_audio_manager(config_proxy, api, &error);
    check_error(error);
}

gchar *
dbus_get_audio_manager(void)
{
    gchar *api;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_audio_manager(config_proxy, &api, &error);
    check_error(error);

    return api;
}

#ifdef SFL_VIDEO
gchar *
dbus_get_active_video_device_channel()
{
    gchar *str = NULL;
    GError *error = NULL;

    org_sflphone_SFLphone_VideoControls_get_active_device_channel(video_proxy, &str, &error);
    check_error(error);

    return str;
}

gchar *
dbus_get_active_video_device_size()
{
    gchar *str = NULL;
    GError *error = NULL;

    org_sflphone_SFLphone_VideoControls_get_active_device_size(video_proxy, &str, &error);
    check_error(error);

    return str;
}

gchar *
dbus_get_active_video_device_rate()
{
    gchar *str = NULL;
    GError *error = NULL;

    org_sflphone_SFLphone_VideoControls_get_active_device_rate(video_proxy, &str, &error);
    check_error(error);

    return str;
}

gchar *
dbus_get_active_video_device()
{
    gchar *str = NULL;
    GError *error = NULL;

    org_sflphone_SFLphone_VideoControls_get_active_device(video_proxy, &str, &error);
    check_error(error);

    return str;
}

void
dbus_set_active_video_device(const gchar *device)
{
    GError *error = NULL;
    org_sflphone_SFLphone_VideoControls_set_active_device(video_proxy, device, &error);
    check_error(error);
}

void
dbus_set_active_video_device_channel(const gchar *channel)
{
    GError *error = NULL;
    org_sflphone_SFLphone_VideoControls_set_active_device_channel(video_proxy, channel, &error);
    check_error(error);
}

void
dbus_set_active_video_device_size(const gchar *size)
{
    GError *error = NULL;
    org_sflphone_SFLphone_VideoControls_set_active_device_size(video_proxy, size, &error);
    check_error(error);
}

void
dbus_set_active_video_device_rate(const gchar *rate)
{
    GError *error = NULL;
    org_sflphone_SFLphone_VideoControls_set_active_device_rate(video_proxy, rate, &error);
    check_error(error);
}

gchar **
dbus_get_video_device_list()
{
    gchar **array = NULL;
    GError *error = NULL;

    org_sflphone_SFLphone_VideoControls_get_device_list(video_proxy, &array, &error);
    check_error(error);
    return array;
}

/**
 * Get the list of channels supported by the given device
 */
gchar **
dbus_get_video_device_channel_list(const gchar *dev)
{
    gchar **array = NULL;
    GError *error = NULL;
    org_sflphone_SFLphone_VideoControls_get_device_channel_list(video_proxy, dev, &array, &error);
    check_error(error);
    return array;
}

/**
 * Get the list of resolutions supported by the given channel of the given device
 */
gchar **
dbus_get_video_device_size_list(const gchar *dev, const gchar *channel)
{
    gchar **array = NULL;
    GError *error = NULL;

    org_sflphone_SFLphone_VideoControls_get_device_size_list(video_proxy, dev, channel, &array, &error);
    check_error(error);
    return array;
}

/**
 * Get the list of frame rates supported by the given resolution of the given channel of the given device
 */
gchar **
dbus_get_video_device_rate_list(const gchar *dev, const gchar *channel, const gchar *size)
{
    gchar **array = NULL;
    GError *error = NULL;

    org_sflphone_SFLphone_VideoControls_get_device_rate_list(video_proxy, dev, channel, size, &array, &error);
    check_error(error);
    return array;
}
#endif

GHashTable *
dbus_get_addressbook_settings(void)
{
    GError *error = NULL;
    GHashTable *results = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_addressbook_settings(config_proxy, &results, &error);
    check_error(error);

    return results;
}

void
dbus_set_addressbook_settings(GHashTable *settings)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_addressbook_settings(config_proxy, settings, &error);
    check_error(error);
}

gchar **
dbus_get_addressbook_list(void)
{
    GError *error = NULL;
    gchar **array = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_addressbook_list(config_proxy, &array, &error);
    check_error(error);

    return array;
}

void
dbus_set_addressbook_list(const gchar **list)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_addressbook_list(config_proxy, list, &error);

    check_error(error);
}

GHashTable *
dbus_get_hook_settings(void)
{
    GError *error = NULL;
    GHashTable *results = NULL;

    org_sflphone_SFLphone_ConfigurationManager_get_hook_settings(config_proxy, &results, &error);
    check_error(error);

    return results;
}

void
dbus_set_hook_settings(GHashTable *settings)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_hook_settings(config_proxy, settings, &error);
    check_error(error);
}

GHashTable *
dbus_get_call_details(const gchar *callID)
{
    GError *error = NULL;
    GHashTable *details = NULL;
    org_sflphone_SFLphone_CallManager_get_call_details(call_proxy, callID, &details, &error);
    check_error(error);

    return details;
}

gchar **
dbus_get_call_list(void)
{
    GError *error = NULL;
    gchar **list = NULL;
    org_sflphone_SFLphone_CallManager_get_call_list(call_proxy, &list, &error);
    check_error(error);

    return list;
}

gchar **
dbus_get_conference_list(void)
{
    GError *error = NULL;
    gchar **list = NULL;
    org_sflphone_SFLphone_CallManager_get_conference_list(call_proxy, &list, &error);
    check_error(error);

    return list;
}

gchar **
dbus_get_participant_list(const gchar *confID)
{
    GError *error = NULL;
    gchar **list = NULL;

    DEBUG("Get conference %s participant list", confID);
    org_sflphone_SFLphone_CallManager_get_participant_list(call_proxy, confID, &list, &error);
    check_error(error);

    return list;
}

gchar *
dbus_get_conference_id(const gchar *callID)
{
    gchar *confID = NULL;
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_get_conference_id(call_proxy, callID, &confID, &error);
    check_error(error);
    return confID;
}

GHashTable *
dbus_get_conference_details(const gchar *confID)
{
    GError *error = NULL;
    GHashTable *details = NULL;
    org_sflphone_SFLphone_CallManager_get_conference_details(call_proxy, confID, &details, &error);
    check_error(error);

    return details;
}

void
dbus_set_accounts_order(const gchar *order)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_accounts_order(config_proxy, order, &error);
    check_error(error);
}

GPtrArray *
dbus_get_history(void)
{
    GError *error = NULL;
    GPtrArray *entries = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_history(config_proxy, &entries, &error);
    check_error(error);

    return entries;
}

void
dbus_confirm_sas(const callable_obj_t *c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_set_sa_sverified(call_proxy, c->_callID, &error);
    check_error(error);
}

void
dbus_reset_sas(const callable_obj_t *c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_reset_sa_sverified(call_proxy, c->_callID, &error);
    check_error(error);
}

void
dbus_set_confirm_go_clear(const callable_obj_t *c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_set_confirm_go_clear(call_proxy, c->_callID, &error);
    check_error(error);
}

void
dbus_request_go_clear(const callable_obj_t *c)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_request_go_clear(call_proxy, c->_callID, &error);
    check_error(error);
}

gchar **
dbus_get_supported_tls_method()
{
    GError *error = NULL;
    gchar **array = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_supported_tls_method(config_proxy, &array, &error);
    check_error(error);

    return array;
}

GHashTable *
dbus_get_tls_settings_default(void)
{
    GError *error = NULL;
    GHashTable *results = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_tls_settings_default(config_proxy, &results, &error);
    check_error(error);

    return results;
}

gchar *
dbus_get_address_from_interface_name(const gchar *interface)
{
    GError *error = NULL;
    gchar *address = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_addr_from_interface_name(config_proxy, interface, &address, &error);
    check_error(error);

    return address;
}

gchar **
dbus_get_all_ip_interface(void)
{
    GError *error = NULL;
    gchar **array = NULL;

    org_sflphone_SFLphone_ConfigurationManager_get_all_ip_interface(config_proxy, &array, &error);
    check_error(error);

    return array;
}

gchar **
dbus_get_all_ip_interface_by_name(void)
{
    GError *error = NULL;
    gchar **array = NULL;

    org_sflphone_SFLphone_ConfigurationManager_get_all_ip_interface_by_name(config_proxy, &array, &error);
    check_error(error);

    return array;
}

GHashTable *
dbus_get_shortcuts(void)
{
    GError *error = NULL;
    GHashTable *shortcuts = NULL;

    org_sflphone_SFLphone_ConfigurationManager_get_shortcuts(config_proxy, &shortcuts, &error);
    check_error(error);

    return shortcuts;
}

void
dbus_set_shortcuts(GHashTable *shortcuts)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_shortcuts(config_proxy, shortcuts, &error);
    check_error(error);
}

void
dbus_send_text_message(const gchar *callID, const gchar *message)
{
    GError *error = NULL;
    org_sflphone_SFLphone_CallManager_send_text_message(call_proxy, callID, message, &error);
    check_error(error);
}

#ifdef SFL_VIDEO
static void
video_preview_async_cb(DBusGProxy *proxy UNUSED, GError *error, gpointer userdata UNUSED)
{
    check_error(error);
    // Reactivate it now that we're done, D-Bus wise
    set_preview_button_sensitivity(TRUE);
}

void
dbus_start_video_preview()
{
    set_preview_button_sensitivity(FALSE);
    org_sflphone_SFLphone_VideoControls_start_preview_async(video_proxy, video_preview_async_cb, NULL);
}

void
dbus_stop_video_preview()
{
    set_preview_button_sensitivity(FALSE);
    org_sflphone_SFLphone_VideoControls_stop_preview_async(video_proxy, video_preview_async_cb, NULL);
}

gboolean
dbus_has_video_preview_started()
{
    GError *error = NULL;
    gboolean started = FALSE;
    org_sflphone_SFLphone_VideoControls_has_preview_started(video_proxy, &started, &error);
    check_error(error);
    return started;
}
#endif

static guint cookie;
#define GNOME_SESSION_NO_IDLE_FLAG 8

static void screensaver_inhibit_cb(GObject * source_object, GAsyncResult * res,
                                   gpointer user_data UNUSED)
{
    GDBusProxy *proxy = G_DBUS_PROXY(source_object);
    GError *error = NULL;
    GVariant *value = g_dbus_proxy_call_finish(proxy, res, &error);
    if (!value) {
        ERROR("%s", error->message);
        g_error_free(error);
        return;
    }

    /* save the cookie */
    if (g_variant_is_of_type(value, G_VARIANT_TYPE("(u)")))
        g_variant_get(value, "(u)", &cookie);
    else
        cookie = 0;

    g_variant_unref(value);
}

static void screensaver_uninhibit_cb(GObject * source_object,
                                     GAsyncResult * res,
                                     gpointer user_data UNUSED)
{
    GDBusProxy *proxy = G_DBUS_PROXY(source_object);
    GError *error = NULL;

    GVariant *value = g_dbus_proxy_call_finish(proxy, res, &error);
    if (!value) {
        ERROR ("%s",
               error->message);
        g_error_free(error);
        return;
    }

    /* clear the cookie */
    cookie = 0;
    g_variant_unref(value);
}

void dbus_screensaver_inhibit(void)
{
    const gchar *appname = g_get_application_name();
    if (appname == NULL) {
        ERROR("could not retrieve application name");
        return;
    }

    guint xid = 0;
    GVariant *parameters = g_variant_new("(susu)", appname, xid,
                                         "Phone call ongoing",
                                         GNOME_SESSION_NO_IDLE_FLAG);
    if (parameters == NULL) {
        ERROR("Could not create session manager inhibit parameters");
        return;
    }

    g_dbus_proxy_call(session_manager_proxy, "Inhibit", parameters,
                      G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL,
                      screensaver_inhibit_cb, NULL);
}

void
dbus_screensaver_uninhibit(void)
{
    if (cookie == 0)
        return;
    DEBUG("uninhibit");

    GVariant *parameters = g_variant_new("(u)", cookie);
    if (parameters == NULL) {
        ERROR("Could not create session manager uninhibit "
               "parameters");
        return;
    }

    g_dbus_proxy_call(session_manager_proxy, "Uninhibit",
                      g_variant_new("(u)", cookie),
                      G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL,
                      screensaver_uninhibit_cb, NULL);
}
