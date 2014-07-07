/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
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
#include "calltab.h"
#include "callmanager-glue.h"
#include "configurationmanager-glue.h"
#ifdef SFL_VIDEO
#include "videomanager-glue.h"
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
#include "accountconfigdialog.h"
#include "messaging/message_tab.h"
#include "sflphone_client.h"
#include "dbus.h"
#include "actions.h"

#ifdef SFL_PRESENCE
#include "presence.h"
#include "presencemanager-glue.h"
#include "presencewindow.h"
#endif

#ifdef SFL_VIDEO
#include "config/videoconf.h"
#include "video/video_callbacks.h"
#endif
#include "account_schema.h"
#include "mainwindow.h"

#ifdef SFL_VIDEO
static DBusGProxy *video_proxy;
#endif
static DBusGProxy *call_proxy;
static DBusGProxy *config_proxy;
static DBusGProxy *instance_proxy;
#ifdef SFL_PRESENCE
static DBusGProxy *presence_proxy;
#endif
static GDBusProxy *session_manager_proxy;

/* Returns TRUE if there was an error, FALSE otherwise */
static gboolean check_error(GError *error)
{
    if (error) {
        g_warning("%s", error->message);
        if (g_error_matches(error, DBUS_GERROR, DBUS_GERROR_SERVICE_UNKNOWN)) {
            g_error_free(error);
            g_warning("daemon crashed, quitting rudely...");
            exit(EXIT_FAILURE);
        }
        g_error_free(error);
        return TRUE;
    }
    return FALSE;
}

static void
new_call_created_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *accountID,
                    const gchar *callID, const gchar *to, SFLPhoneClient *client)
{
    callable_obj_t *c = create_new_call(CALL, CALL_STATE_RINGING, callID,
                                        accountID, to, to);

    calllist_add_call(current_calls_tab, c);
    calltree_add_call(current_calls_tab, c, NULL);

    update_actions(client);
    calltree_display(current_calls_tab, client);
}

static void
incoming_call_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *accountID,
                 const gchar *callID, const gchar *from, SFLPhoneClient *client)
{
    // We receive the from field under a formatted way. We want to extract the number and the name of the caller
    gchar *display_name = call_get_display_name(from);
    gchar *peer_number = call_get_peer_number(from);

    callable_obj_t *c = create_new_call(CALL, CALL_STATE_INCOMING, callID,
                                        accountID, display_name, peer_number);

    g_free(peer_number);
    g_free(display_name);

    /* Legacy system tray option, requires TopIcons GNOME extension */
    status_tray_icon_blink(TRUE);
    if (g_settings_get_boolean(client->settings, "popup-main-window"))
        popup_main_window(client);

    if (g_settings_get_boolean(client->settings, "bring-window-to-front"))
        main_window_bring_to_front(client, c->_time_start);

    notify_incoming_call(c, client);
    sflphone_incoming_call(c, client);
}

static void
zrtp_negotiation_failed_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *callID,
                           const gchar *reason, const gchar *severity,
                           SFLPhoneClient *client)
{
    main_window_zrtp_negotiation_failed(callID, reason, severity, client);
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c)
        notify_zrtp_negotiation_failed(c, client);
}

static void
volume_changed_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *device, gdouble value,
                  G_GNUC_UNUSED gpointer foo)
{
    set_slider_no_update(device, value);
}

static void
voice_mail_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *accountID, guint nb,
              SFLPhoneClient *client)
{
    sflphone_notify_voice_mail(accountID, nb, client);
}

static void
incoming_message_cb(G_GNUC_UNUSED DBusGProxy *proxy, const G_GNUC_UNUSED gchar *callID,
                    const G_GNUC_UNUSED gchar *from, const gchar *msg, SFLPhoneClient *client)
{
    // do not display message if instant messaging is disabled
    if (!g_settings_get_boolean(client->settings, "instant-messaging-enabled"))
        return;

    callable_obj_t *call = calllist_get_call(current_calls_tab, callID);

    if (call) {
        new_text_message(call, msg, client);
    } else {
        conference_obj_t *conf = conferencelist_get(current_calls_tab, callID);
        if (!conf) {
            g_warning("Message received, but no recipient found");
            return;
        }

        new_text_message_conf(conf, msg, from, client);
    }
}

/**
 * Perform the right sflphone action based on the requested state
 */
static void
process_existing_call_state_change(callable_obj_t *c, const gchar *state, SFLPhoneClient *client)
{
    if (c == NULL) {
        g_warning("Pointer to call is NULL in %s\n", __func__);
        return;
    } else if (state == NULL) {
        g_warning("Pointer to state is NULL in %s\n", __func__);
        return;
    }

    if (g_strcmp0(state, "HUNGUP") == 0) {
        if (c->_state == CALL_STATE_CURRENT)
            time(&c->_time_stop);

        calltree_update_call(history_tab, c, client);
        status_bar_display_account();
        sflphone_hung_up(c, client);
    } else if (g_strcmp0(state, "UNHOLD") == 0 || g_strcmp0(state, "CURRENT") == 0)
        sflphone_current(c, client);
    else if (g_strcmp0(state, "HOLD") == 0)
        sflphone_hold(c, client);
    else if (g_strcmp0(state, "RINGING") == 0)
        sflphone_ringing(c, client);
    else if (g_strcmp0(state, "FAILURE") == 0)
        sflphone_fail(c, client);
    else if (g_strcmp0(state, "BUSY") == 0)
        sflphone_busy(c, client);
}


/**
 * This function process call state changes in case the call have not been created yet.
 * This mainly occurs when another SFLphone client takes actions.
 */
static void
process_nonexisting_call_state_change(const gchar *callID, const gchar *state, SFLPhoneClient *client)
{
    if (callID == NULL) {
        g_warning("Pointer to call id is NULL in %s\n", __func__);
        return;
    } else if (state == NULL) {
        g_warning("Pointer to state is NULL in %s\n", __func__);
        return;
    } else if (g_strcmp0(state, "HUNGUP") == 0)
        return; // Could occur if a user picked up the phone and hung up without making a call

    // The callID is unknown, treat it like a new call
    // If it were an incoming call, we won't be here
    // It means that a new call has been initiated with an other client (cli for instance)
    if (g_strcmp0(state, "RINGING") == 0 || g_strcmp0(state, "CURRENT") == 0) {

        g_debug("New ringing call! accountID: %s", callID);

        restore_call(callID);
        callable_obj_t *new_call = calllist_get_call(current_calls_tab, callID);
        if (new_call)
            calltree_add_call(current_calls_tab, new_call, NULL);
        update_actions(client);
        calltree_display(current_calls_tab, client);
    }
}

static void
call_state_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *callID,
              const gchar *state, SFLPhoneClient *client)
{
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c)
        process_existing_call_state_change(c, state, client);
    else {
        g_warning("Call does not exist");
        process_nonexisting_call_state_change(callID, state, client);
    }
}

static void
toggle_im(conference_obj_t *conf, G_GNUC_UNUSED gboolean activate)
{
    for (GSList *p = conf->participant_list; p; p = g_slist_next(p)) {
        //callable_obj_t *call = calllist_get_call(current_calls_tab, p->data);

        /*TODO elepage(2012) Implement IM messaging toggle here*/
    }
}

static void
conference_changed_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *confID,
                      const gchar *state, SFLPhoneClient *client)
{
    g_debug("Conference state changed: %s\n", state);

    conference_obj_t* changed_conf = conferencelist_get(current_calls_tab, confID);
    if (changed_conf == NULL) {
        g_warning("Conference is NULL in conference state changed");
        return;
    }

    // remove old conference from calltree
    calltree_remove_conference(current_calls_tab, changed_conf, client);

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
        g_debug("Error: conference state not recognized");

    // reactivate instant messaging window for these calls
    toggle_im(changed_conf, TRUE);

    gchar **list = dbus_get_participant_list(changed_conf->_confID);
    conference_participant_list_update(list, changed_conf);
    g_strfreev(list);

    // deactivate instant messaging window for new participants
    toggle_im(changed_conf, FALSE);
    calltree_add_conference_to_current_calls(changed_conf, client);
}

static void
conference_created_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *confID, SFLPhoneClient *client)
{
    g_debug("Conference %s added", confID);

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
    calltree_add_conference_to_current_calls(new_conf, client);
}

static void
conference_removed_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *confID,
                      SFLPhoneClient *client)
{
    g_debug("Conference removed %s", confID);
    conference_obj_t *c = conferencelist_get(current_calls_tab, confID);
    if(c == NULL) {
        g_warning("Could not find conference %s from list", confID);
        return;
    }

    calltree_remove_conference(current_calls_tab, c, client);

    /*TODO elepage(2012) implement unmerging of IM here*/

    // remove all participants for this conference
    for (GSList *p = c->participant_list; p; p = g_slist_next(p)) {
        //callable_obj_t *call = calllist_get_call(current_calls_tab, p->data);
        /*TODO elepage(2012) implement unmerging of IM here*/
    }

    conferencelist_remove(current_calls_tab, c->_confID);
}

static void
record_playback_filepath_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *id,
                            const gchar *filepath)
{
    g_debug("Filepath for %s: %s", id, filepath);
    callable_obj_t *call = calllist_get_call(current_calls_tab, id);
    conference_obj_t *conf = conferencelist_get(current_calls_tab, id);

    if (call && conf) {
        g_warning("Two objects for this callid");
        return;
    }

    if (!call && !conf) {
        g_warning("Could not get object");
        return;
    }

    if (call && call->_recordfile == NULL)
        call->_recordfile = g_strdup(filepath);
    else if (conf && conf->_recordfile == NULL)
        conf->_recordfile = g_strdup(filepath);
}

static void
record_playback_stopped_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *filepath, SFLPhoneClient *client)
{
    g_debug("Playback stopped for %s", filepath);
    const gint calllist_size = calllist_get_size(history_tab);

    for (gint i = 0; i < calllist_size; i++) {
        callable_obj_t *call = calllist_get_nth(history_tab, i);

        if (call == NULL) {
            g_warning("Could not find %dth call", i);
            break;
        }
        if (g_strcmp0(call->_recordfile, filepath) == 0)
            call->_record_is_playing = FALSE;
    }

    update_actions(client);
}

static void
update_playback_scale_cb(G_GNUC_UNUSED DBusGProxy *proxy,
        const gchar *file_path, guint position, guint size)
{
    if (!main_window_update_playback_scale(file_path, position, size))
        update_ringtone_slider(position, size);
}

static void
registration_state_changed_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *accountID,
                              guint state, G_GNUC_UNUSED void *foo)
{
    g_debug("DBus: Registration state changed to %s for account %s",
          account_state_name(state), accountID);
    account_t *acc = account_list_get_by_id(accountID);
    if (acc) {
        acc->state = state;
        update_account_list_status_bar(acc);
        status_bar_display_account();
    }
}

static void
accounts_changed_cb(G_GNUC_UNUSED DBusGProxy *proxy, G_GNUC_UNUSED void *foo)
{
    g_debug("Account details changed.");
    sflphone_fill_account_list();
    sflphone_fill_ip2ip_profile();

    // ui updates
    status_bar_display_account();
    statusicon_set_tooltip();
#ifdef SFL_PRESENCE
    update_presence_statusbar();
#endif
}

static void
stun_status_failure_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *accountID, G_GNUC_UNUSED void *foo)
{
    g_warning("Error: Stun status failure: account %s failed to setup STUN",
          accountID);
    // Disable STUN for the account that tried to create the STUN transport
    account_t *account = account_list_get_by_id(accountID);
    if (account) {
        account_replace(account, CONFIG_STUN_ENABLE, "false");
        dbus_set_account_details(account);
    }
}

static void
stun_status_success_cb(G_GNUC_UNUSED DBusGProxy *proxy, G_GNUC_UNUSED const gchar *message, G_GNUC_UNUSED void *foo)
{
    g_debug("STUN setup successful");
}

static void
transfer_succeeded_cb(G_GNUC_UNUSED DBusGProxy *proxy, G_GNUC_UNUSED void *foo)
{
    sflphone_display_transfer_status("Transfer successful");
}

static void
transfer_failed_cb(G_GNUC_UNUSED DBusGProxy *proxy, G_GNUC_UNUSED void *foo)
{
    sflphone_display_transfer_status("Transfer failed");
}

static void
secure_sdes_on_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *callID, SFLPhoneClient *client)
{
    g_debug("SRTP using SDES is on");
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c) {
        sflphone_srtp_sdes_on(c, client);
        notify_secure_on(c, client);
    }
}

static void
secure_sdes_off_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *callID, SFLPhoneClient *client)
{
    g_debug("SRTP using SDES is off");
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c) {
        sflphone_srtp_sdes_off(c, client);
        notify_secure_off(c, client);
    }
}

static void
secure_zrtp_on_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *callID,
                  const gchar *cipher, SFLPhoneClient *client)
{
    g_debug("SRTP using ZRTP is ON with cipher %s", cipher);
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c) {
        sflphone_srtp_zrtp_on(c, client);
        notify_secure_on(c, client);
    }
}

static void
secure_zrtp_off_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *callID, SFLPhoneClient *client)
{
    g_debug("SRTP using ZRTP is OFF");
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c) {
        sflphone_srtp_zrtp_off(c, client);
        notify_secure_off(c, client);
    }
}

static void
show_zrtp_sas_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *callID, const gchar *sas,
                 gboolean verified, SFLPhoneClient *client)
{
    g_debug("Showing SAS");
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c)
        sflphone_srtp_zrtp_show_sas(c, sas, verified, client);
}

static void
confirm_go_clear_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *callID, SFLPhoneClient *client)
{
    g_debug("Confirm Go Clear request");
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c)
        main_window_confirm_go_clear(c, client);
}

static void
zrtp_not_supported_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *callID, SFLPhoneClient *client)
{
    g_debug("ZRTP not supported on the other end");
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c) {
        main_window_zrtp_not_supported(c, client);
        notify_zrtp_not_supported(c, client);
    }
}


#ifdef RTCP_DEBUG
static void
print_rtcp_stats(const gchar *key, gint value, G_GNUC_UNUSED gpointer data)
{
    g_debug("%s: %d", key, value);
}
#endif

static void
on_rtcp_report_received_cb(G_GNUC_UNUSED DBusGProxy *proxy,
        const gchar *callID,
        G_GNUC_UNUSED const GHashTable *stats,
        G_GNUC_UNUSED SFLPhoneClient *client)
{
    g_debug("Daemon notification of new RTCP report for %s", callID);
#ifdef RTCP_DEBUG
    g_hash_table_foreach(stats, print_rtcp_stats, NULL);
#endif
}

static void
sip_call_state_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *callID,
                  const gchar *description, guint code, SFLPhoneClient *client)
{
    g_debug("Sip call state changed %s", callID);
    callable_obj_t *c = calllist_get_call(current_calls_tab, callID);

    if (c)
        sflphone_call_state_changed(c, description, code, client);
}

static void
error_alert(G_GNUC_UNUSED DBusGProxy *proxy, int err, G_GNUC_UNUSED void *foo)
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

    g_warning("%s", msg);
}

static void
screensaver_dbus_proxy_new_cb (G_GNUC_UNUSED GObject * source, GAsyncResult *result, G_GNUC_UNUSED gpointer user_data)
{
    g_debug("Session manager connection callback");

    session_manager_proxy = g_dbus_proxy_new_for_bus_finish (result, NULL);
    if (session_manager_proxy == NULL)
        g_warning("could not initialize gnome session manager");
}

#ifdef SFL_PRESENCE
static void
sip_presence_subscription_state_changed_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *accID,
                              const gchar *uri, gboolean state, G_GNUC_UNUSED void *foo)
{
    g_debug("DBus: Presence subscription state changed to %s for account %s, buddy:%s",
          state? "active":"inactive", accID, uri);
    account_t *acc = account_list_get_by_id(accID);
    if (acc)
    {
        buddy_t * b = presence_buddy_list_buddy_get_by_string(accID, uri);
        if(b)
        {
            b->subscribed = state;
            if(!state) // not monitored means default value for status ( == Offline)
            {
                b->status = FALSE;
                g_free(b->note);
                b->note = g_strdup("Not found");
            }
            update_presence_view();
        }
    }
}

static void
sip_presence_notification_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *accID, const gchar *uri,
                  gboolean status, const gchar * note)
{
    g_debug("DBus: Presence notification (%s) for %s status=%s lineStatus=%s.",
            accID, uri, status? PRESENCE_STATUS_ONLINE:PRESENCE_STATUS_OFFLINE, note);

    account_t *acc = account_list_get_by_id(accID);
    if (acc)
    {
        gchar *my_uri = g_strconcat("<sip:",
                account_lookup(acc, CONFIG_ACCOUNT_USERNAME),
                "@",
                account_lookup(acc, CONFIG_ACCOUNT_HOSTNAME),
                ">", NULL);

        if ((g_strcmp0(uri, my_uri) == 0)) // self-subscription
        {
            account_replace(acc, CONFIG_PRESENCE_STATUS, status? PRESENCE_STATUS_ONLINE:PRESENCE_STATUS_OFFLINE);
            account_replace(acc, CONFIG_PRESENCE_NOTE, note);
            update_presence_statusbar();
        }
        else
        {
            buddy_t * b = presence_buddy_list_buddy_get_by_string(accID, uri);
            if(b)
            {
                b->status = status;
                g_free(b->note);
                b->note = g_strdup(note);
                update_presence_view();
            }
        }
    }
}

static void
sip_presence_server_error_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *accID, const gchar *err, const gchar *msg)
{
    g_debug("DBus: Presence error from server (%s) : %s / %s.",accID, err, msg);
}

static void
sip_presence_new_subscription_request_cb(G_GNUC_UNUSED DBusGProxy *proxy, const gchar *uri)
{
    g_debug("DBus: Presence new subscription from %s.",uri);
}
#endif

#define GS_SERVICE   "org.gnome.SessionManager"
#define GS_PATH      "/org/gnome/SessionManager"
#define GS_INTERFACE "org.gnome.SessionManager"

gboolean dbus_connect_session_manager(DBusGConnection *connection)
{

    if (connection == NULL) {
        g_warning("connection is NULL");
        return FALSE;
    }
/*
    session_manager_proxy = dbus_g_proxy_new_for_name(connection,
                            "org.gnome.SessionManager", "/org/gnome/SessionManager/Inhibitor",
                            "org.gnome.SessionManager.Inhibitor");

    if(session_manager_proxy == NULL) {
        g_warning("Error, could not create session manager proxy");
        return FALSE;
    }
*/

    g_dbus_proxy_new_for_bus(G_BUS_TYPE_SESSION,
                             G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES,
                             NULL, GS_SERVICE, GS_PATH, GS_INTERFACE, NULL,
                             screensaver_dbus_proxy_new_cb, NULL);

    g_debug("Connected to gnome session manager");

    return TRUE;
}

gboolean dbus_connect(GError **error, SFLPhoneClient *client)
{
    const char *dbus_message_bus_name = "org.sflphone.SFLphone";
    const char *dbus_object_instance = "/org/sflphone/SFLphone/Instance";
    const char *dbus_interface = "org.sflphone.SFLphone.Instance";
    const char *callmanager_object_instance = "/org/sflphone/SFLphone/CallManager";
    const char *callmanager_interface = "org.sflphone.SFLphone.CallManager";
    const char *configurationmanager_object_instance = "/org/sflphone/SFLphone/ConfigurationManager";
    const char *configurationmanager_interface = "org.sflphone.SFLphone.ConfigurationManager";
#ifdef SFL_PRESENCE
    const char *presencemanager_object_instance = "/org/sflphone/SFLphone/PresenceManager";
    const char *presencemanager_interface = "org.sflphone.SFLphone.PresenceManager";
#endif

    DBusGConnection *connection = dbus_g_bus_get(DBUS_BUS_SESSION, error);
    if (connection == NULL) {
        g_warning("could not establish connection with session bus");
        return FALSE;
    }

    /* Create a proxy object for the "bus driver" (name "org.freedesktop.DBus") */
    g_debug("Connect to message bus:     %s", dbus_message_bus_name);
    g_debug("           object instance: %s", dbus_object_instance);
    g_debug("           dbus interface:  %s", dbus_interface);

    instance_proxy = dbus_g_proxy_new_for_name(connection, dbus_message_bus_name, dbus_object_instance, dbus_interface);
    if (instance_proxy == NULL) {
        g_warning("Error: Failed to connect to %s", dbus_message_bus_name);
        return FALSE;
    }

    g_debug("Connect to object instance: %s", callmanager_object_instance);
    g_debug("           dbus interface:  %s", callmanager_interface);

    call_proxy = dbus_g_proxy_new_for_name(connection, dbus_message_bus_name, callmanager_object_instance, callmanager_interface);
    if (call_proxy == NULL) {
        g_warning("Error: Failed to connect to %s", callmanager_object_instance);
        return FALSE;
    }

    config_proxy = dbus_g_proxy_new_for_name(connection, dbus_message_bus_name, configurationmanager_object_instance, configurationmanager_interface);
    if (config_proxy == NULL) {
        g_warning("Error: Failed to connect to %s", configurationmanager_object_instance);
        return FALSE;
    }

#ifdef SFL_PRESENCE
    presence_proxy = dbus_g_proxy_new_for_name(connection, dbus_message_bus_name, presencemanager_object_instance, presencemanager_interface);
    if (presence_proxy == NULL) {
        g_warning("Error: Failed to connect to %s", presencemanager_object_instance);
        return FALSE;
    }
#endif

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

    /* Register STRING STRING INT INT BOOL Marshaller */
    dbus_g_object_register_marshaller(
        g_cclosure_user_marshal_VOID__STRING_STRING_INT_INT_BOOLEAN, G_TYPE_NONE,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_BOOLEAN,
        G_TYPE_INVALID);

    /* Register STRING STRING BOOLEAN STRING Marshaller */
    dbus_g_object_register_marshaller(
        g_cclosure_user_marshal_VOID__STRING_STRING_BOOLEAN_STRING, G_TYPE_NONE,
        G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_INVALID);

    g_debug("Adding callmanager Dbus signals");

    /* Incoming call */
    dbus_g_proxy_add_signal(call_proxy, "newCallCreated", G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "newCallCreated",
                                G_CALLBACK(new_call_created_cb), client, NULL);

    dbus_g_proxy_add_signal(call_proxy, "incomingCall", G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "incomingCall",
                                G_CALLBACK(incoming_call_cb), client, NULL);

    dbus_g_proxy_add_signal(call_proxy, "zrtpNegotiationFailed",
                            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "zrtpNegotiationFailed",
                                G_CALLBACK(zrtp_negotiation_failed_cb), client, NULL);

    dbus_g_proxy_add_signal(call_proxy, "callStateChanged", G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "callStateChanged",
                                G_CALLBACK(call_state_cb), client, NULL);

    dbus_g_proxy_add_signal(call_proxy, "voiceMailNotify", G_TYPE_STRING,
                            G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "voiceMailNotify",
                                G_CALLBACK(voice_mail_cb), client, NULL);

    dbus_g_proxy_add_signal(config_proxy, "registrationStateChanged", G_TYPE_STRING,
                            G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(config_proxy, "registrationStateChanged",
                                G_CALLBACK(registration_state_changed_cb), NULL, NULL);

    dbus_g_proxy_add_signal(call_proxy, "incomingMessage", G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "incomingMessage",
                                G_CALLBACK(incoming_message_cb), client, NULL);

    dbus_g_proxy_add_signal(config_proxy, "volumeChanged", G_TYPE_STRING,
                            G_TYPE_DOUBLE, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(config_proxy, "volumeChanged",
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
                                G_CALLBACK(conference_changed_cb), client, NULL);

    dbus_g_proxy_add_signal(call_proxy, "conferenceCreated", G_TYPE_STRING,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "conferenceCreated",
                                G_CALLBACK(conference_created_cb), client, NULL);

    dbus_g_proxy_add_signal(call_proxy, "conferenceRemoved", G_TYPE_STRING,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "conferenceRemoved",
                                G_CALLBACK(conference_removed_cb), client, NULL);

    /* Playback related signals */
    dbus_g_proxy_add_signal(call_proxy, "recordPlaybackFilepath", G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "recordPlaybackFilepath",
                                G_CALLBACK(record_playback_filepath_cb), client, NULL);

    dbus_g_proxy_add_signal(call_proxy, "recordPlaybackStopped", G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "recordPlaybackStopped",
                                G_CALLBACK(record_playback_stopped_cb), client, NULL);

    dbus_g_proxy_add_signal(call_proxy, "updatePlaybackScale", G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "updatePlaybackScale",
                                G_CALLBACK(update_playback_scale_cb), NULL, NULL);

    /* Security related callbacks */
    dbus_g_proxy_add_signal(call_proxy, "secureSdesOn", G_TYPE_STRING,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "secureSdesOn",
                                G_CALLBACK(secure_sdes_on_cb), client, NULL);

    dbus_g_proxy_add_signal(call_proxy, "secureSdesOff", G_TYPE_STRING,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "secureSdesOff",
                                G_CALLBACK(secure_sdes_off_cb), client, NULL);

    dbus_g_proxy_add_signal(call_proxy, "showSAS", G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "showSAS",
                                G_CALLBACK(show_zrtp_sas_cb), client, NULL);

    dbus_g_proxy_add_signal(call_proxy, "secureZrtpOn", G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "secureZrtpOn",
                                G_CALLBACK(secure_zrtp_on_cb), client, NULL);

    dbus_g_proxy_add_signal(call_proxy, "secureZrtpOff", G_TYPE_STRING,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "secureZrtpOff",
                                G_CALLBACK(secure_zrtp_off_cb), client, NULL);

    dbus_g_proxy_add_signal(call_proxy, "zrtpNotSuppOther", G_TYPE_STRING,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "zrtpNotSuppOther",
                                G_CALLBACK(zrtp_not_supported_cb), client, NULL);

    dbus_g_proxy_add_signal(call_proxy, "confirmGoClear", G_TYPE_STRING,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "confirmGoClear",
                                G_CALLBACK(confirm_go_clear_cb), client, NULL);

    dbus_g_proxy_add_signal(call_proxy, "sipCallStateChanged",
                            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INT,
                            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(call_proxy, "sipCallStateChanged",
                                G_CALLBACK(sip_call_state_cb), client, NULL);


    /* Manually register this marshaller as we need to declare that the boxed type is in fact
     * a GHashTable */
    dbus_g_object_register_marshaller(g_cclosure_user_marshal_VOID__STRING_BOXED, G_TYPE_NONE,
            G_TYPE_STRING, G_TYPE_HASH_TABLE, G_TYPE_INVALID);

    dbus_g_proxy_add_signal(call_proxy, "onRtcpReportReceived",
                            G_TYPE_STRING,
                            dbus_g_type_get_map("GHashTable", G_TYPE_STRING, G_TYPE_INT),
                            G_TYPE_INVALID);

    dbus_g_proxy_connect_signal(call_proxy, "onRtcpReportReceived",
                                G_CALLBACK(on_rtcp_report_received_cb), client, NULL);

    g_debug("Adding configurationmanager Dbus signals");

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

#ifdef SFL_PRESENCE
    g_debug("Adding presencemanager Dbus signals");

    /* Presence related callbacks */
    dbus_g_proxy_add_signal(presence_proxy, "subscriptionStateChanged", G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(presence_proxy, "subscriptionStateChanged",
                                G_CALLBACK(sip_presence_subscription_state_changed_cb), client, NULL);

    dbus_g_proxy_add_signal(presence_proxy, "newBuddyNotification", G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(presence_proxy, "newBuddyNotification",
                                G_CALLBACK(sip_presence_notification_cb), client, NULL);

    dbus_g_proxy_add_signal(presence_proxy, "serverError", G_TYPE_STRING,G_TYPE_STRING,
                            G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(presence_proxy, "serverError",
                                G_CALLBACK(sip_presence_server_error_cb), NULL, NULL);

    dbus_g_proxy_add_signal(presence_proxy, "newServerSubscriptionRequest", G_TYPE_STRING, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(presence_proxy, "newServerSubscriptionRequest",
                                G_CALLBACK(sip_presence_new_subscription_request_cb), NULL, NULL);
#endif

#ifdef SFL_VIDEO
    const gchar *videomanager_object_instance = "/org/sflphone/SFLphone/VideoManager";
    const gchar *videomanager_interface = "org.sflphone.SFLphone.VideoManager";
    video_proxy = dbus_g_proxy_new_for_name(connection, dbus_message_bus_name,
            videomanager_object_instance, videomanager_interface);
    if (video_proxy == NULL) {
        g_warning("Error: Failed to connect to %s", videomanager_object_instance);
        return FALSE;
    }
    /* Video related signals */
    dbus_g_proxy_add_signal(video_proxy, "deviceEvent", G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(video_proxy, "deviceEvent",
            G_CALLBACK(video_device_event_cb), NULL, NULL);

    dbus_g_proxy_add_signal(video_proxy, "startedDecoding", G_TYPE_STRING,
            G_TYPE_STRING, G_TYPE_INT, G_TYPE_INT, G_TYPE_BOOLEAN,
            G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(video_proxy, "startedDecoding",
            G_CALLBACK(started_decoding_video_cb), client, NULL);

    dbus_g_proxy_add_signal(video_proxy, "stoppedDecoding",
            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_INVALID);
    dbus_g_proxy_connect_signal(video_proxy, "stoppedDecoding",
            G_CALLBACK(stopped_decoding_video_cb), client, NULL);
#endif

    /* Defines a default timeout for the proxies */
    static const int DEFAULT_DBUS_TIMEOUT = 30000;
    dbus_g_proxy_set_default_timeout(call_proxy, DEFAULT_DBUS_TIMEOUT);
    dbus_g_proxy_set_default_timeout(instance_proxy, DEFAULT_DBUS_TIMEOUT);
    dbus_g_proxy_set_default_timeout(config_proxy, DEFAULT_DBUS_TIMEOUT);
#ifdef SFL_PRESENCE
    dbus_g_proxy_set_default_timeout(presence_proxy, DEFAULT_DBUS_TIMEOUT);
#endif
#ifdef SFL_VIDEO
    dbus_g_proxy_set_default_timeout(video_proxy, DEFAULT_DBUS_TIMEOUT);
#endif

    gboolean status = dbus_connect_session_manager(connection);
    if(status == FALSE) {
        g_warning("could not connect to gnome session manager");
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
#ifdef SFL_PRESENCE
    g_object_unref(presence_proxy);
#endif
}

void dbus_hold(const callable_obj_t *c)
{
    GError *error = NULL;
    gboolean result;
    org_sflphone_SFLphone_CallManager_hold(call_proxy, c->_callID, &result, &error);
    check_error(error);
}

void
dbus_unhold(const callable_obj_t *c)
{
    GError *error = NULL;
    gboolean result;
    org_sflphone_SFLphone_CallManager_unhold(call_proxy, c->_callID, &result, &error);
    check_error(error);
}

void
dbus_hold_conference(const conference_obj_t *c)
{
    GError *error = NULL;
    gboolean result;
    org_sflphone_SFLphone_CallManager_hold_conference(call_proxy, c->_confID,
            &result, &error);
    check_error(error);
}

void
dbus_unhold_conference(const conference_obj_t *c)
{
    GError *error = NULL;
    gboolean result;
    org_sflphone_SFLphone_CallManager_unhold_conference(call_proxy, c->_confID,
            &result, &error);
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
hang_up_reply_cb(G_GNUC_UNUSED DBusGProxy *proxy,
        G_GNUC_UNUSED gboolean is_hung_up,
        GError *error,
        G_GNUC_UNUSED gpointer userdata)
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
    gboolean result;
    org_sflphone_SFLphone_CallManager_transfer(call_proxy, c->_callID,
            c->_trsft_to, &result, &error);
    check_error(error);
}

void
dbus_attended_transfer(const callable_obj_t *transfer, const callable_obj_t *target)
{
    GError *error = NULL;
    gboolean result;
    org_sflphone_SFLphone_CallManager_attended_transfer(call_proxy, transfer->_callID,
                           target->_callID, &result, &error);
    check_error(error);
}

void
dbus_accept(const callable_obj_t *c)
{
    status_tray_icon_blink(FALSE);
    GError *error = NULL;
    gboolean result;
    org_sflphone_SFLphone_CallManager_accept(call_proxy, c->_callID, &result, &error);
    check_error(error);
}

void
dbus_refuse(const callable_obj_t *c)
{
    status_tray_icon_blink(FALSE);
    GError *error = NULL;
    gboolean result;
    org_sflphone_SFLphone_CallManager_refuse(call_proxy, c->_callID, &result, &error);
    check_error(error);
}

void
dbus_place_call(const callable_obj_t *c)
{
    GError *error = NULL;
    gboolean result;
    org_sflphone_SFLphone_CallManager_place_call(call_proxy, c->_accountID,
            c->_callID, c->_peer_number, &result, &error);
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
dbus_get_account_template()
{
    GError *error = NULL;
    GHashTable *details = NULL;

    org_sflphone_SFLphone_ConfigurationManager_get_account_template(config_proxy, &details, &error);
    check_error(error);

    return details;
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
dbus_get_ip2ip_details(void)
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
    org_sflphone_SFLphone_ConfigurationManager_set_volume(config_proxy, device, value, &error);
    check_error(error);
}

void
dbus_mute_capture(gboolean muted)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_mute_capture(config_proxy, muted, &error);
    check_error(error);
}

void
dbus_mute_dtmf(gboolean muted)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_mute_dtmf(config_proxy, muted, &error);
    check_error(error);
}

void
dbus_mute_playback(gboolean muted)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_mute_playback(config_proxy, muted, &error);
    check_error(error);
}

gboolean
dbus_is_dtmf_muted()
{
    GError *error = NULL;
    gboolean muted;
    org_sflphone_SFLphone_ConfigurationManager_is_dtmf_muted(config_proxy, &muted, &error);
    check_error(error);
    return muted;
}

gboolean
dbus_is_capture_muted()
{
    GError *error = NULL;
    gboolean muted;
    org_sflphone_SFLphone_ConfigurationManager_is_capture_muted(config_proxy, &muted, &error);
    check_error(error);
    return muted;
}

gboolean
dbus_is_playback_muted()
{
    GError *error = NULL;
    gboolean muted;
    org_sflphone_SFLphone_ConfigurationManager_is_playback_muted(config_proxy, &muted, &error);
    check_error(error);
    return muted;
}

gdouble
dbus_get_volume(const gchar *device)
{
    gdouble value;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_volume(config_proxy, device, &value, &error);
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
    org_sflphone_SFLphone_VideoManager_get_codecs(video_proxy, accountID, &array, &error);
    check_error(error);
    return array;
}

void
dbus_set_video_codecs(const gchar *accountID, const GPtrArray *list)
{
    GError *error = NULL;
    org_sflphone_SFLphone_VideoManager_set_codecs(video_proxy, accountID, list, &error);
    check_error(error);
}

gboolean
dbus_switch_video_input(const gchar *resource)
{
    GError *error = NULL;
    gboolean switched;
    org_sflphone_SFLphone_VideoManager_switch_input(video_proxy, resource, &switched, &error);
    check_error(error);
    return switched;
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
dbus_get_audio_output_device_index(const gchar *name)
{
    int device_index = 0;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_audio_output_device_index(config_proxy, name, &device_index, &error);
    check_error(error);

    return device_index;
}

/**
 * Get index
 */
int
dbus_get_audio_input_device_index(const gchar *name)
{
    int device_index = 0;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_audio_input_device_index(config_proxy, name, &device_index, &error);
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
gboolean
dbus_get_noise_suppress_state()
{
    gboolean state;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_noise_suppress_state(config_proxy, &state, &error);

    if (check_error(error))
        state = FALSE;

    return state;
}

/**
 * Set noise reduction state
 */
void
dbus_set_noise_suppress_state(gboolean state)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_noise_suppress_state(config_proxy, state, &error);
    check_error(error);
}

/**
 * Get AGC state
 */
gboolean
dbus_get_agc_state()
{
    gboolean state;
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_is_agc_enabled(config_proxy, &state, &error);

    if (check_error(error))
        state = FALSE;

    return state;
}

/**
 * Set AGC state
 */
void
dbus_set_agc_state(gboolean state)
{
    GError *error = NULL;
    org_sflphone_SFLphone_ConfigurationManager_set_agc_state(config_proxy, state, &error);
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

static void
dbus_join_participant_async_cb(G_GNUC_UNUSED DBusGProxy *proxy,
                      gboolean result,
                      GError *error,
                      G_GNUC_UNUSED gpointer data)
{
    check_error(error);
    if (!result)
        g_warning("Failed to join participant");
}

void
dbus_join_participant(const gchar *sel_callID, const gchar *drag_callID)
{
    org_sflphone_SFLphone_CallManager_join_participant_async(call_proxy, sel_callID,
            drag_callID, dbus_join_participant_async_cb, NULL);
}

static void
dbus_add_participant_async_cb(G_GNUC_UNUSED DBusGProxy *proxy,
                              gboolean result,
                              GError *error,
                              G_GNUC_UNUSED gpointer data)
{
    check_error(error);
    if (!result)
        g_warning("Failed to add participant");
}

void
dbus_add_participant(const gchar *callID, const gchar *confID)
{
    org_sflphone_SFLphone_CallManager_add_participant_async(call_proxy, callID,
            confID, dbus_add_participant_async_cb, NULL);
}

static void
dbus_add_main_participant_async_cb(G_GNUC_UNUSED DBusGProxy *proxy,
                                   gboolean result,
                                   GError *error,
                                   G_GNUC_UNUSED gpointer data)
{
    check_error(error);
    if (!result)
        g_warning("Failed to add main participant");
}

void
dbus_add_main_participant(const gchar *confID)
{
    org_sflphone_SFLphone_CallManager_add_main_participant_async(call_proxy,
            confID, dbus_add_main_participant_async_cb, NULL);
}

void
dbus_detach_participant(const gchar *callID)
{
    GError *error = NULL;
    gboolean result;
    org_sflphone_SFLphone_CallManager_detach_participant(call_proxy, callID,
            &result, &error);
    check_error(error);
}

void
dbus_join_conference(const gchar *sel_confID, const gchar *drag_confID)
{
    GError *error = NULL;
    gboolean result;
    org_sflphone_SFLphone_CallManager_join_conference(call_proxy, sel_confID,
            drag_confID, &result, &error);
    check_error(error);
}

gboolean
dbus_toggle_recording(const gchar *id)
{
    GError *error = NULL;
    gboolean isRecording;
    org_sflphone_SFLphone_CallManager_toggle_recording(call_proxy, id, &isRecording, &error);
    check_error(error);
    return isRecording;
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

gboolean
dbus_set_audio_manager(const gchar *api)
{
    GError *error = NULL;
    gboolean result;
    org_sflphone_SFLphone_ConfigurationManager_set_audio_manager(config_proxy, api, &result, &error);
    check_error(error);
    return result;
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

gchar **
dbus_get_supported_audio_managers()
{
    GError *error = NULL;
    gchar **array = NULL;
    org_sflphone_SFLphone_ConfigurationManager_get_supported_audio_managers(config_proxy, &array, &error);
    check_error(error);

    return array;
}

#ifdef SFL_VIDEO
gchar *
dbus_video_get_default_device()
{
    gchar *str = NULL;
    GError *error = NULL;

    org_sflphone_SFLphone_VideoManager_get_default_device(video_proxy, &str, &error);
    check_error(error);

    return str;
}

void
dbus_video_set_default_device(const gchar *name)
{
    GError *error = NULL;
    org_sflphone_SFLphone_VideoManager_set_default_device(video_proxy, name, &error);
    check_error(error);
}

GHashTable *
dbus_video_get_settings(const gchar *name)
{
    GError *error = NULL;
    GHashTable *settings = NULL;

    org_sflphone_SFLphone_VideoManager_get_settings(video_proxy, name, &settings, &error);
    check_error(error);

    return settings;
}

void
dbus_video_apply_settings(const gchar *name, GHashTable *settings)
{
    GError *error = NULL;
    org_sflphone_SFLphone_VideoManager_apply_settings(video_proxy, name, settings, &error);
    check_error(error);
}

gchar **
dbus_video_get_device_list()
{
    gchar **array = NULL;
    GError *error = NULL;

    org_sflphone_SFLphone_VideoManager_get_device_list(video_proxy, &array, &error);
    check_error(error);
    return array;
}

GHashTable *
dbus_video_get_capabilities(const gchar *name)
{
    GError *error = NULL;
    GHashTable *cap = NULL;

    org_sflphone_SFLphone_VideoManager_get_capabilities(video_proxy, name, &cap, &error);
    check_error(error);

    return cap;
}
#endif


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
dbus_get_display_names(const gchar *confID)
{
    GError *error = NULL;
    gchar **list = NULL;

    org_sflphone_SFLphone_CallManager_get_display_names(call_proxy, confID, &list, &error);
    check_error(error);

    return list;
}

gchar **
dbus_get_participant_list(const gchar *confID)
{
    GError *error = NULL;
    gchar **list = NULL;

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

static void
get_history_async_cb(G_GNUC_UNUSED DBusGProxy *proxy, GPtrArray *items, GError *error, gpointer userdata)
{
    IdleData *id = userdata;
    check_error(error);
    id->items = items;
    id->dbus_finished = TRUE;
}

void
dbus_get_history(IdleData *id)
{
    org_sflphone_SFLphone_ConfigurationManager_get_history_async(config_proxy, get_history_async_cb, id);
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

gboolean
dbus_check_certificate(const gchar *capath, const gchar *certpath)
{
    GError *error = NULL;
    gboolean result;
    org_sflphone_SFLphone_ConfigurationManager_check_certificate_validity(config_proxy, capath, certpath, &result, &error);
    check_error(error);

    return result;
}

gboolean
dbus_certificate_contains_private_key(const gchar *filepath)
{
    GError *error = NULL;
    gboolean result;
    org_sflphone_SFLphone_ConfigurationManager_check_for_private_key(config_proxy, filepath, &result, &error);
    check_error(error);

    return result;
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
video_camera_async_cb(G_GNUC_UNUSED DBusGProxy *proxy, GError *error, G_GNUC_UNUSED gpointer userdata)
{
    check_error(error);
    // Reactivate it now that we're done, D-Bus wise
    set_camera_button_sensitivity(TRUE);
}

void
dbus_start_video_camera()
{
    set_camera_button_sensitivity(FALSE);
    org_sflphone_SFLphone_VideoManager_start_camera_async(video_proxy, video_camera_async_cb, NULL);
}

void
dbus_stop_video_camera()
{
    set_camera_button_sensitivity(FALSE);
    org_sflphone_SFLphone_VideoManager_stop_camera_async(video_proxy, video_camera_async_cb, NULL);
}

gboolean
dbus_has_video_camera_started()
{
    GError *error = NULL;
    gboolean started = FALSE;
    org_sflphone_SFLphone_VideoManager_has_camera_started(video_proxy, &started, &error);
    check_error(error);
    return started;
}
#endif

static guint cookie;
#define GNOME_SESSION_NO_IDLE_FLAG 8

static void screensaver_inhibit_cb(GObject * source_object, GAsyncResult * res,
                                   G_GNUC_UNUSED gpointer user_data)
{
    GDBusProxy *proxy = G_DBUS_PROXY(source_object);
    GError *error = NULL;
    GVariant *value = g_dbus_proxy_call_finish(proxy, res, &error);
    if (!value) {
        g_warning("%s", error->message);
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
                                     G_GNUC_UNUSED gpointer user_data)
{
    GDBusProxy *proxy = G_DBUS_PROXY(source_object);
    GError *error = NULL;

    GVariant *value = g_dbus_proxy_call_finish(proxy, res, &error);
    if (!value) {
        g_warning("%s", error->message);
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
        g_warning("could not retrieve application name");
        return;
    }

    guint xid = 0;
    GVariant *parameters = g_variant_new("(susu)", appname, xid,
                                         "Phone call ongoing",
                                         GNOME_SESSION_NO_IDLE_FLAG);
    if (parameters == NULL) {
        g_warning("Could not create session manager inhibit parameters");
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

    GVariant *parameters = g_variant_new("(u)", cookie);
    if (parameters == NULL) {
        g_warning("Could not create session manager uninhibit parameters");
        return;
    }

    g_dbus_proxy_call(session_manager_proxy, "Uninhibit",
                      g_variant_new("(u)", cookie),
                      G_DBUS_CALL_FLAGS_NO_AUTO_START, -1, NULL,
                      screensaver_uninhibit_cb, NULL);
}

#ifdef SFL_PRESENCE
void
dbus_presence_publish(const gchar *accountID, gboolean status)
{
    GError *error = NULL;
    org_sflphone_SFLphone_PresenceManager_publish(presence_proxy, accountID,status, "Tout va bien.", NULL);
    check_error(error);
}

void
dbus_presence_subscribe(const gchar *accountID, const gchar *uri, gboolean flag)
{
    GError *error = NULL;
    org_sflphone_SFLphone_PresenceManager_subscribe_buddy(presence_proxy, accountID, uri, flag, NULL);
    check_error(error);
}
#endif
