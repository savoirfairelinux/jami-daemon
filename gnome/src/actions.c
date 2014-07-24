/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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
#include <gtk/gtk.h>

#include "str_utils.h"
#include "uimanager.h"
#include "mainwindow.h"
#include "calltree.h"
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <arpa/nameser.h>
#include <netinet/in.h>
#include <resolv.h>

#include <linux/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include "actions.h"
#include "dbus/dbus.h"
#include "account_schema.h"
#include "contacts/calltab.h"
#include "contacts/searchbar.h"
#include "contacts/addrbookfactory.h"
#include "icons/icon_factory.h"
#include "statusicon.h"
#include "sliders.h"
#include "messaging/message_tab.h"
#include "history_loader.h"

static GHashTable * ip2ip_profile;

void
sflphone_notify_voice_mail(const gchar* accountID , guint count, SFLPhoneClient *client)
{
    // We want to notify only the current account; ie the first in the list
    gchar *id = g_strdup(accountID);
    const gchar * const current_id = account_list_get_current_id();

    g_debug("sflphone_notify_voice_mail begin");

    if (g_ascii_strcasecmp(id, current_id) != 0 ||
        account_list_get_size() == 0)
        return;

    // Set the number of voice messages for the current account
    current_account_set_message_number(count);
    account_t *current = account_list_get_current();

    // Update the voicemail tool button
    update_voicemail_status();

    if (current)
        notify_voice_mails(count, current, client);

    g_debug("sflphone_notify_voice_mail end");
}

/*
 * Place a call with the current account.
 * If there is no default account selected, place a call with the first
 * registered account of the account list
 * Else, check if it an IP call. if not, popup an error message
 */

static gboolean is_direct_call(callable_obj_t * c)
{
    if (utf8_case_equal(c->_accountID, "empty")) {
        if (!g_str_has_prefix(c->_peer_number, "sip:")) {
            gchar * new_number = g_strconcat("sip:", c->_peer_number, NULL);
            g_free(c->_peer_number);
            c->_peer_number = new_number;
        }

        return TRUE;
    }

    return g_str_has_prefix(c->_peer_number, "sip:") ||
           g_str_has_prefix(c->_peer_number, "sips:");
}


void
status_bar_display_account()
{
    statusbar_pop_message(__MSG_ACCOUNT_DEFAULT);

    account_t *acc = account_list_get_current();
    status_tray_icon_online(acc != NULL);

    static const char * const account_types[] = {
        N_("(SIP)"),
        N_("(IAX)")
    };

    gchar* msg;
    if (acc) {
        const guint type_idx = account_is_IAX(acc);
        msg = g_markup_printf_escaped(_("%s %s"),
                                      (gchar*) account_lookup(acc, CONFIG_ACCOUNT_ALIAS),
                                      _(account_types[type_idx]));
    } else {
        msg = g_markup_printf_escaped(_("No registered accounts"));
    }

    statusbar_push_message(msg, NULL, __MSG_ACCOUNT_DEFAULT);
    g_free(msg);
}


void
sflphone_quit(gboolean force_quit, SFLPhoneClient *client)
{
    if (force_quit || calllist_get_size(current_calls_tab) == 0 || main_window_ask_quit(client)) {
        dbus_unregister(getpid());
        dbus_clean();
        account_list_free();
        calllist_clean(current_calls_tab);
        calllist_clean(contacts_tab);
        calllist_clean(history_tab);
        free_addressbook();

#if GLIB_CHECK_VERSION(2,32,0)
        g_application_quit(G_APPLICATION(client));
#else
        g_application_release(G_APPLICATION(client));
#endif
    }
}

void
sflphone_hold(callable_obj_t * c, SFLPhoneClient *client)
{
    c->_state = CALL_STATE_HOLD;
    calltree_update_call(current_calls_tab, c, client);
}

void
sflphone_ringing(callable_obj_t * c, SFLPhoneClient *client)
{
    c->_state = CALL_STATE_RINGING;
    calltree_update_call(current_calls_tab, c, client);
}

void
sflphone_hung_up(callable_obj_t * c, SFLPhoneClient *client)
{
    disable_messaging_tab(c->_callID);
    calllist_remove_call(current_calls_tab, c->_callID, client);
    calltree_remove_call(current_calls_tab, c->_callID);
    c->_state = CALL_STATE_DIALING;
    call_remove_all_errors(c);
    update_actions(client);

    status_tray_icon_blink(FALSE);

    statusbar_update_clock("");
}

void sflphone_fill_account_list(void)
{
    account_list_init();
    gchar **array = dbus_account_list();

    for (gchar **accountID = array; accountID && *accountID; ++accountID) {
        account_t *acc = create_account_with_ID(*accountID);
        if (acc->properties == NULL) {
            g_warning("SFLphone: Error: Could not fetch details for account %s",
                      *accountID);
            break;
        }
        account_list_add(acc);
        /* Fill the actual array of credentials */
        dbus_get_credentials(acc);
        gchar * status = account_lookup(acc, CONFIG_ACCOUNT_REGISTRATION_STATUS);

        if (g_strcmp0(status, "REGISTERED") == 0)
            acc->state = ACCOUNT_STATE_REGISTERED;
        else if (g_strcmp0(status, "UNREGISTERED") == 0)
            acc->state = ACCOUNT_STATE_UNREGISTERED;
        else if (g_strcmp0(status, "TRYING") == 0)
            acc->state = ACCOUNT_STATE_TRYING;
        else if (g_strcmp0(status, "ERROR_GENERIC") == 0)
            acc->state = ACCOUNT_STATE_ERROR;
        else if (g_strcmp0(status, "ERROR_AUTH") == 0)
            acc->state = ACCOUNT_STATE_ERROR_AUTH;
        else if (g_strcmp0(status, "ERROR_NETWORK") == 0)
            acc->state = ACCOUNT_STATE_ERROR_NETWORK;
        else if (g_strcmp0(status, "ERROR_HOST") == 0)
            acc->state = ACCOUNT_STATE_ERROR_HOST;
        else if (g_strcmp0(status, "ERROR_NOT_ACCEPTABLE") == 0)
            acc->state = ACCOUNT_STATE_ERROR_NOT_ACCEPTABLE;
        else if (g_strcmp0(status, "ERROR_EXIST_STUN") == 0)
            acc->state = ACCOUNT_STATE_ERROR_EXIST_STUN;
        else if (g_strcmp0(status, "READY") == 0)
            acc->state = ACCOUNT_STATE_IP2IP_READY;
        else {
            g_warning("Unexpected status %s", status);
            acc->state = ACCOUNT_STATE_INVALID;
        }

        gchar * code = account_lookup(acc, CONFIG_ACCOUNT_REGISTRATION_STATE_CODE);
        if (code != NULL)
            acc->protocol_state_code = atoi(code);
        acc->protocol_state_description = account_lookup(acc, CONFIG_ACCOUNT_REGISTRATION_STATE_DESC);
    }

    g_strfreev(array);

    // Set the current account message number
    current_account_set_message_number(current_account_get_message_number());
}

gboolean sflphone_init(GError **error, SFLPhoneClient *client)
{
    if (!dbus_connect(error, client) || !dbus_register(getpid(), "Gtk+ Client", error))
        return FALSE;

    abook_init();

    // Init icons factory
    init_icon_factory();

    current_calls_tab = calltab_init(FALSE, CURRENT_CALLS, client);
    contacts_tab = calltab_init(TRUE, CONTACTS, client);
    history_tab = calltab_init(TRUE, HISTORY, client);

    codecs_load();
    conferencelist_init(current_calls_tab);

    // Fetch the configured accounts
    sflphone_fill_account_list();

    // Fetch the ip2ip profile
    sflphone_fill_ip2ip_profile();

    return TRUE;
}

void sflphone_fill_ip2ip_profile(void)
{
    ip2ip_profile = dbus_get_ip2ip_details();
}

GHashTable *sflphone_get_ip2ip_properties(void)
{
    return ip2ip_profile;
}

void
sflphone_hang_up(SFLPhoneClient *client)
{
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls_tab);
    conference_obj_t * selectedConf = calltab_get_selected_conf(active_calltree_tab);

    if (selectedConf) {
        disable_messaging_tab(selectedConf->_confID);
        dbus_hang_up_conference(selectedConf);
    } else if (selectedCall) {
        switch (selectedCall->_state) {
            case CALL_STATE_DIALING:
                dbus_hang_up(selectedCall);
                break;
            case CALL_STATE_RINGING:
                dbus_hang_up(selectedCall);
                call_remove_all_errors(selectedCall);
                selectedCall->_state = CALL_STATE_DIALING;
                //selectedCall->_stop = 0;
                break;
            case CALL_STATE_CURRENT:
            case CALL_STATE_HOLD:
            case CALL_STATE_BUSY:
                dbus_hang_up(selectedCall);
                call_remove_all_errors(selectedCall);
                selectedCall->_state = CALL_STATE_DIALING;
                time(&selectedCall->_time_stop);

                break;
            case CALL_STATE_FAILURE:
                dbus_hang_up(selectedCall);
                call_remove_all_errors(selectedCall);
                selectedCall->_state = CALL_STATE_DIALING;
                break;
            case CALL_STATE_INCOMING:
                dbus_refuse(selectedCall);
                call_remove_all_errors(selectedCall);
                selectedCall->_state = CALL_STATE_DIALING;
                g_debug("from sflphone_hang_up : ");
                break;
            case CALL_STATE_TRANSFER:
                dbus_hang_up(selectedCall);
                call_remove_all_errors(selectedCall);
                time(&selectedCall->_time_stop);
                break;
            default:
                g_warning("Should not happen in sflphone_hang_up()!");
                break;
        }
    }

    calltree_update_call(history_tab, selectedCall, client);

    statusbar_update_clock("");

    // Allow screen saver to start
    if (calllist_get_size(current_calls_tab) == 1)
        dbus_screensaver_uninhibit();
}

void
sflphone_pick_up(SFLPhoneClient *client)
{
    callable_obj_t *selectedCall = calltab_get_selected_call(active_calltree_tab);

    // Disable screensaver if the list is empty call
    if (calllist_get_size(current_calls_tab) == 0)
        dbus_screensaver_inhibit();

    if (!selectedCall) {
        sflphone_new_call(client);
        return;
    }

    switch (selectedCall->_state) {
        case CALL_STATE_DIALING:
            sflphone_place_call(selectedCall, client);
            break;
        case CALL_STATE_INCOMING:
            selectedCall->_history_state = g_strdup(INCOMING_STRING);
            calltree_update_call(history_tab, selectedCall, client);

            dbus_accept(selectedCall);
            break;
        case CALL_STATE_TRANSFER:
            dbus_transfer(selectedCall);
            break;
        case CALL_STATE_CURRENT:
        case CALL_STATE_HOLD:
        case CALL_STATE_RINGING:
            sflphone_new_call(client);
            break;
        default:
            g_warning("Should not happen in sflphone_pick_up()!");
            break;
    }
}

void
sflphone_on_hold()
{
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls_tab);
    conference_obj_t * selectedConf = calltab_get_selected_conf(active_calltree_tab);

    if (selectedCall) {
        switch (selectedCall->_state) {
            case CALL_STATE_CURRENT:
                dbus_hold(selectedCall);
                break;
            default:
                g_warning("Should not happen in sflphone_on_hold!");
                break;
        }
    } else if (selectedConf)
        dbus_hold_conference(selectedConf);
}

void
sflphone_off_hold()
{
    g_debug("%s", __PRETTY_FUNCTION__);
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls_tab);
    conference_obj_t * selectedConf = calltab_get_selected_conf(active_calltree_tab);

    if (selectedCall) {
        switch (selectedCall->_state) {
            case CALL_STATE_HOLD:
                dbus_unhold(selectedCall);
                break;
            default:
                g_warning("Should not happen in sflphone_off_hold ()!");
                break;
        }
    } else if (selectedConf)
        dbus_unhold_conference(selectedConf);
}


void
sflphone_fail(callable_obj_t * c, SFLPhoneClient *client)
{
    c->_state = CALL_STATE_FAILURE;
    calltree_update_call(current_calls_tab, c, client);
}

void
sflphone_busy(callable_obj_t * c, SFLPhoneClient *client)
{
    c->_state = CALL_STATE_BUSY;
    calltree_update_call(current_calls_tab, c, client);
}

void
sflphone_current(callable_obj_t * c, SFLPhoneClient *client)
{
    if (c->_state != CALL_STATE_HOLD)
        time(&c->_time_start);

    c->_state = CALL_STATE_CURRENT;
    calltree_update_call(current_calls_tab, c, client);
}

void
sflphone_set_transfer(SFLPhoneClient *client)
{
    callable_obj_t * c = calltab_get_selected_call(current_calls_tab);

    if (c) {
        c->_state = CALL_STATE_TRANSFER;
        g_free(c->_trsft_to);
        c->_trsft_to = g_strdup("");
        calltree_update_call(current_calls_tab, c, client);
    } else {
        update_actions(client);
    }
}

void
sflphone_unset_transfer(SFLPhoneClient *client)
{
    callable_obj_t * c = calltab_get_selected_call(current_calls_tab);

    if (c) {
        c->_state = CALL_STATE_CURRENT;
        g_free(c->_trsft_to);
        c->_trsft_to = g_strdup("");
        calltree_update_call(current_calls_tab, c, client);
    } else {
        update_actions(client);
    }
}

void
sflphone_display_transfer_status(const gchar* message)
{
    statusbar_push_message(message , NULL, __MSG_ACCOUNT_DEFAULT);
}

void
sflphone_incoming_call(callable_obj_t * c, SFLPhoneClient *client)
{
    c->_history_state = g_strdup(MISSED_STRING);
    calllist_add_call(current_calls_tab, c);
    calltree_add_call(current_calls_tab, c, NULL);

    update_actions(client);
    calltree_display(current_calls_tab, client);

    // Change the status bar if we are dealing with a direct SIP call
    if (is_direct_call(c)) {
        gchar *msg = g_markup_printf_escaped(_("Direct SIP call"));
        statusbar_pop_message(__MSG_ACCOUNT_DEFAULT);
        statusbar_push_message(msg , NULL, __MSG_ACCOUNT_DEFAULT);
        g_free(msg);
    }
    account_t *account = account_list_get_by_id(c->_accountID);
    if (!account) {
        g_warning("Account is NULL");
    } else if (account_has_autoanswer_on(account)) {
        calltab_select_call(active_calltree_tab, c);
        sflphone_pick_up(client);
    }
}

static void
process_dialing(callable_obj_t *c, guint keyval, const gchar *key, SFLPhoneClient *client)
{
    // We stop the tone
    if (!*c->_peer_number && c->_state != CALL_STATE_TRANSFER)
        dbus_start_tone(FALSE, 0);

    switch (keyval) {
        case GDK_KEY_Return:
        case GDK_KEY_KP_Enter:
            sflphone_place_call(c, client);
            break;
        case GDK_KEY_Escape:
            sflphone_hang_up(client);
            break;
        case GDK_KEY_BackSpace: {
            gchar *num = (c->_state == CALL_STATE_TRANSFER) ? c->_trsft_to : c->_peer_number;
            size_t len = strlen(num);

            if (len) {
                len--; // delete one character
                num[len] = '\0';
                calltree_update_call(current_calls_tab, c, client);

                /* If number is now empty, hang up immediately */
                if (c->_state != CALL_STATE_TRANSFER && len == 0)
                    dbus_hang_up(c);
            }

            break;
        }
        case GDK_KEY_Tab:
        case GDK_KEY_Alt_L:
        case GDK_KEY_Alt_R:
        case GDK_KEY_Control_L:
        case GDK_KEY_Control_R:
        case GDK_KEY_Super_L:
        case GDK_KEY_Super_R:
        case GDK_KEY_Caps_Lock:
            break;
        default:

            if (keyval < 127 /* ascii */ ||
                    (keyval >= GDK_KEY_Mode_switch && keyval <= GDK_KEY_KP_9) /* num keypad */) {

                if (c->_state == CALL_STATE_TRANSFER) {
                    gchar *new_trsft = g_strconcat(c->_trsft_to, key, NULL);
                    g_free(c->_trsft_to);
                    c->_trsft_to = new_trsft;
                } else {
                    dbus_play_dtmf(key);
                    gchar *new_peer_number = g_strconcat(c->_peer_number, key, NULL);
                    g_free(c->_peer_number);
                    c->_peer_number = new_peer_number;
                }

                calltree_update_call(current_calls_tab, c, client);
            }

            break;
    }
}


callable_obj_t *
sflphone_new_call(SFLPhoneClient *client)
{
    // Disable screensaver if the list is empty call
    guint nbcall = calllist_get_size(current_calls_tab);
    if(nbcall == 0)
        dbus_screensaver_inhibit();

    callable_obj_t *selected_call = calllist_empty(current_calls_tab) ? NULL :
        calltab_get_selected_call(current_calls_tab);

    if (selected_call != NULL && selected_call->_callID) {
        gchar *confID = dbus_get_conference_id(selected_call->_callID);
        if (g_strcmp0(confID, "") != 0)
            sflphone_on_hold();
        g_free(confID);
    }

    // Play a tone when creating a new call
    if (calllist_empty(current_calls_tab))
        dbus_start_tone(TRUE , (current_account_has_new_message()  > 0) ? TONE_WITH_MESSAGE : TONE_WITHOUT_MESSAGE) ;

    callable_obj_t *c = create_new_call(CALL, CALL_STATE_DIALING, "", "", "", "");

    c->_history_state = g_strdup(OUTGOING_STRING);

    calllist_add_call(current_calls_tab, c);
    calltree_add_call(current_calls_tab, c, NULL);
    update_actions(client);

    return c;
}


void
sflphone_keypad(guint keyval, const gchar * key, SFLPhoneClient *client)
{
    callable_obj_t * c = calllist_empty(current_calls_tab) ? NULL :
                         calltab_get_selected_call(current_calls_tab);

    const gboolean current_is_active_tab = calltab_has_name(active_calltree_tab, CURRENT_CALLS);
    if (!current_is_active_tab || (current_is_active_tab && !c)) {
        switch (keyval) {
            case GDK_KEY_Return:
            case GDK_KEY_KP_Enter:
            case GDK_KEY_Escape:
            case GDK_KEY_BackSpace:
                break;
            default:
                calltree_display(current_calls_tab, client);
                process_dialing(sflphone_new_call(client), keyval, key, client);
                break;
        }
    } else if (c) {
        switch (c->_state) {
            case CALL_STATE_DIALING: // Currently dialing => edit number
                process_dialing(c, keyval, key, client);
                break;
            case CALL_STATE_CURRENT:

                switch (keyval) {
                    case GDK_KEY_Escape:
                        dbus_hang_up(c);
                        time(&c->_time_stop);
                        calltree_update_call(history_tab, c, client);
                        break;
                    default:
                        // To play the dtmf when calling mail box for instance
                        dbus_play_dtmf(key);
                        break;
                }

                break;
            case CALL_STATE_INCOMING:

                switch (keyval) {
                    case GDK_KEY_Return:
                    case GDK_KEY_KP_Enter:
                        c->_history_state = g_strdup(INCOMING_STRING);
                        calltree_update_call(history_tab, c, client);
                        dbus_accept(c);
                        break;
                    case GDK_KEY_Escape:
                        dbus_refuse(c);
                        break;
                }

                break;
            case CALL_STATE_TRANSFER:

                switch (keyval) {
                    case GDK_KEY_Return:
                    case GDK_KEY_KP_Enter:
                        dbus_transfer(c);
                        break;
                    case GDK_KEY_Escape:
                        sflphone_unset_transfer(client);
                        break;
                    default: // When a call is on transfer, typing new numbers will add it to c->_peer_number
                        process_dialing(c, keyval, key, client);
                        break;
                }

                break;
            case CALL_STATE_HOLD:

                switch (keyval) {
                    case GDK_KEY_Return:
                    case GDK_KEY_KP_Enter:
                        dbus_unhold(c);
                        break;
                    case GDK_KEY_Escape:
                        dbus_hang_up(c);
                        break;
                    default: // When a call is on hold, typing new numbers will create a new call
                        process_dialing(sflphone_new_call(client), keyval, key, client);
                        break;
                }

                break;
            case CALL_STATE_RINGING:
            case CALL_STATE_BUSY:
            case CALL_STATE_FAILURE:

                switch (keyval) {
                    case GDK_KEY_Escape:
                        dbus_hang_up(c);
                        calltree_update_call(history_tab, c, client);
                        break;
                }

                break;
            default:
                break;
        }
    }
}

int
sflphone_place_call(callable_obj_t * c, SFLPhoneClient *client)
{
    account_t * account = NULL;

    if (c == NULL) {
        g_warning("Callable object is NULL while making new call");
        return -1;
    }

    g_debug("Placing call from %s to %s using account %s", c->_display_name, c->_peer_number, c->_accountID);

    if (c->_state != CALL_STATE_DIALING) {
        g_warning("Call not in state dialing, cannot place call");
        return -1;
    }

    if (!c->_peer_number || strlen(c->_peer_number) == 0) {
        g_warning("No peer number set for this call");
        return -1;
    }

    // Get the account for this call
    if (strlen(c->_accountID) != 0) {
        g_debug("Account %s already set for this call", c->_accountID);
        account = account_list_get_by_id(c->_accountID);
    } else {
        g_debug("No account set for this call, use first of the list");
        account = account_list_get_current();
    }

    // Make sure the previously found account is registered, take first one registered elsewhere
    if (account) {
        const gchar *status = account_lookup(account, CONFIG_ACCOUNT_REGISTRATION_STATUS);
        if (!utf8_case_equal(status, "REGISTERED")) {
            // Place the call with the first registered account
            account = account_list_get_by_state(ACCOUNT_STATE_REGISTERED);
        }
    }

    // If there is no account specified or found, fallback on IP2IP call
    if(account == NULL) {
        g_debug("Could not find an account for this call, making ip to ip call");
        account = account_list_get_by_id("IP2IP");
        if (account == NULL) {
            g_warning("Actions: Could not determine any account for this call");
            return -1;
        }
    }

    // free memory for previous account id and use the new one in case it changed
    g_free(c->_accountID);
    c->_accountID = g_strdup(account->accountID);
    dbus_place_call(c);
    notify_current_account(account, client);

    c->_history_state = g_strdup(OUTGOING_STRING);

    return 0;
}

void
sflphone_detach_participant(const gchar* callID)
{
    callable_obj_t * selectedCall;

    if (callID == NULL)
        selectedCall = calltab_get_selected_call(current_calls_tab);
    else
        selectedCall = calllist_get_call(current_calls_tab, callID);

    g_debug("Detach participant %s", selectedCall->_callID);

    /*TODO elepage(2012) correct IM conversation*/
    calltree_remove_call(current_calls_tab, selectedCall->_callID);
    calltree_add_call(current_calls_tab, selectedCall, NULL);
    dbus_detach_participant(selectedCall->_callID);
}

void
sflphone_add_participant(const gchar* callID, const gchar* confID)
{
    g_debug("Add participant %s to conference %s", callID, confID);

    callable_obj_t *call = calllist_get_call(current_calls_tab, callID);

    if (call == NULL) {
        g_warning("Could not find call");
        return;
    }

    dbus_add_participant(callID, confID);
}

void
sflphone_add_main_participant(const conference_obj_t * c)
{
    g_debug("%s", __PRETTY_FUNCTION__);
    dbus_add_main_participant(c->_confID);
}


gboolean
sflphone_rec_call(SFLPhoneClient *client)
{
    gboolean result = FALSE;
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls_tab);
    conference_obj_t * selectedConf = calltab_get_selected_conf(current_calls_tab);

    if (selectedCall) {
        result = dbus_toggle_recording(selectedCall->_callID);
        calltree_update_call(current_calls_tab, selectedCall, client);
    } else if (selectedConf) {
        result = dbus_toggle_recording(selectedConf->_confID);

        switch (selectedConf->_state) {
            case CONFERENCE_STATE_ACTIVE_ATTACHED:
                selectedConf->_state = CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD;
                break;
            case CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD:
                selectedConf->_state = CONFERENCE_STATE_ACTIVE_ATTACHED;
                break;
            case CONFERENCE_STATE_ACTIVE_DETACHED:
                selectedConf->_state = CONFERENCE_STATE_ACTIVE_DETACHED_RECORD;
                break;
            case CONFERENCE_STATE_ACTIVE_DETACHED_RECORD:
                selectedConf->_state = CONFERENCE_STATE_ACTIVE_DETACHED_RECORD;
                break;
            default:
                g_warning("Should not happen in sflphone_off_hold ()!");
                break;
        }

        g_debug("Remove and add conference %s", selectedConf->_confID);
        calltree_remove_conference(current_calls_tab, selectedConf, client);
        /* This calls update actions */
        calltree_add_conference_to_current_calls(selectedConf, client);
    } else {
        update_actions(client);
    }
    return result;
}

static void
sflphone_fill_audio_codec_list_per_account(account_t *account)
{
    if (!account->acodecs)
        account->acodecs = g_queue_new();
    else
        g_queue_clear(account->acodecs);

    GQueue *system_acodecs = get_audio_codecs_list();
    if (!system_acodecs) {  // should never happen
        g_warning("Couldn't get codec list in %s at %d", __FILE__, __LINE__);
        return;
    }

    /* First add the active codecs for this account */
    GArray *order = dbus_get_active_audio_codec_list(account->accountID);
    if (order) {
        for (guint i = 0; i < order->len; i++) {
            gint payload = g_array_index(order, gint, i);
            codec_t *orig = codec_list_get_by_payload(payload, system_acodecs);
            codec_t *c = codec_create_new_from_caps(orig);

            if (c) {
                c->is_active = TRUE;
                g_queue_push_tail(account->acodecs, c);
            } else
                g_warning("Couldn't find codec %d %p", payload, orig);
        }
        g_array_unref(order);
    } else {
        g_warning("SFLphone: Error: order obj is NULL in %s at %d", __FILE__, __LINE__);
    }

    /* Here we add installed codecs that aren't active for the account */
    guint caps_size = g_queue_get_length(system_acodecs);
    for (guint i = 0; i < caps_size; ++i) {
        codec_t * acodec = g_queue_peek_nth(system_acodecs, i);
        if (codec_list_get_by_payload(acodec->payload, account->acodecs) == NULL) {
            acodec->is_active = FALSE;
            g_queue_push_tail(account->acodecs, acodec);
        }
    }
}

void sflphone_fill_codec_list_per_account(account_t *account)
{
    sflphone_fill_audio_codec_list_per_account(account);
}

void sflphone_fill_call_list(void)
{
    gchar **call_list = dbus_get_call_list();

    for (gchar **callp = call_list; callp && *callp; ++callp) {
        const gchar *callID = *callp;
        if (!calllist_get_call(current_calls_tab, callID)) {
            callable_obj_t *call = create_new_call_from_details(callID, dbus_get_call_details(callID));
            call->_zrtp_confirmed = FALSE;
            calllist_add_call(current_calls_tab, call);

            // only add in treeview if this call is NOT in a conference
            gchar *confID = dbus_get_conference_id(call->_callID);
            if (g_strcmp0(confID, "") == 0)
                calltree_add_call(current_calls_tab, call, NULL);
            g_free(confID);
        }
    }

    g_strfreev(call_list);
}


void sflphone_fill_conference_list(SFLPhoneClient *client)
{
    // TODO Fetch the active conferences at client startup

    gchar **conferences = dbus_get_conference_list();

    for (gchar **list = conferences; list && *list; list++) {
        const gchar * const conf_id = *list;

        GHashTable *conference_details = dbus_get_conference_details(conf_id);
        conference_obj_t *conf = create_new_conference_from_details(conf_id, conference_details);

        conferencelist_add(current_calls_tab, conf);
        calltree_add_conference_to_current_calls(conf, client);
    }

    g_strfreev(conferences);
}

void sflphone_fill_history_lazy()
{
    lazy_load_items(history_tab);
}

void
sflphone_srtp_sdes_on(callable_obj_t * c, SFLPhoneClient *client)
{
    c->_srtp_state = SRTP_STATE_SDES_SUCCESS;

    calltree_update_call(current_calls_tab, c, client);
}

void
sflphone_srtp_sdes_off(callable_obj_t * c, SFLPhoneClient *client)
{
    c->_srtp_state = SRTP_STATE_UNLOCKED;
    calltree_update_call(current_calls_tab, c, client);
}


void
sflphone_srtp_zrtp_on(callable_obj_t * c, SFLPhoneClient *client)
{
    c->_srtp_state = SRTP_STATE_ZRTP_SAS_UNCONFIRMED;

    calltree_update_call(current_calls_tab, c, client);
}

void
sflphone_srtp_zrtp_off(callable_obj_t * c, SFLPhoneClient *client)
{
    c->_srtp_state = SRTP_STATE_UNLOCKED;
    calltree_update_call(current_calls_tab, c, client);
}

void
sflphone_srtp_zrtp_show_sas(callable_obj_t * c, const gchar* sas, const gboolean verified, SFLPhoneClient *client)
{
    c->_sas = g_strdup(sas);
    c->_srtp_state = verified ? SRTP_STATE_ZRTP_SAS_CONFIRMED : SRTP_STATE_ZRTP_SAS_UNCONFIRMED;

    calltree_update_call(current_calls_tab, c, client);
}

void
sflphone_request_go_clear(void)
{
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls_tab);

    if (selectedCall)
        dbus_request_go_clear(selectedCall);
}

void
sflphone_call_state_changed(callable_obj_t * c, const gchar * description, const guint code, SFLPhoneClient *client)
{
    g_debug("Call State changed %s", description);

    if (c == NULL) {
        g_warning("SFLphone: Error: callable obj is NULL in %s at %d", __FILE__, __LINE__);
        return;
    }

    g_free(c->_state_code_description);
    c->_state_code_description = g_strdup(description);
    c->_state_code = code;

    calltree_update_call(current_calls_tab, c, client);
}

#ifdef SFL_VIDEO
gchar *
sflphone_get_display(void)
{
    int width = gdk_screen_width();
    int height = gdk_screen_height();
    char *display = getenv("DISPLAY");

    return g_strdup_printf("display://%s %dx%d", display, width, height);
}

gchar *
sflphone_get_active_video(void)
{
    gchar *device = dbus_video_get_default_device();
    gchar *resource;

    if (strlen(device) > 0)
        resource = g_strconcat("v4l2://", device, NULL);
    else
        resource = sflphone_get_video_none();

    g_free(device);

    return resource;
}

gchar *
sflphone_get_video_none(void)
{
    static const gchar * const logo = "file://" ICONS_DIR "/sflphone.png";
    const gchar *none = access(logo, R_OK) ? "" : logo;

    return g_strdup(none);
}

/*
 * URI of previous resource
 * FIXME when we'll validate the toggle alternatives, we might get rid of this.
 * FIXME this will be leaked at exit
 */
static gchar *last_uri;

void
sflphone_toggle_screenshare(void)
{
    gchar *resource = last_uri && g_str_has_prefix(last_uri, "display://") ?
        sflphone_get_active_video() :
        sflphone_get_display();

    sflphone_switch_video_input(resource);
    g_free(resource);
}

void
sflphone_switch_video_input(const gchar *resource)
{
    gchar *decoded = g_uri_unescape_string(resource, NULL);
    g_debug("MRL: '%s'", decoded);

    if (dbus_switch_video_input(decoded)) {
        g_free(last_uri);
        last_uri = g_strdup(resource);
    } else {
        g_warning("Failed to switch to MRL '%s'\n", resource);
    }

    g_free(decoded);
}
#endif
