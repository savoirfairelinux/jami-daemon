/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
/* Backward compatibility for gtk < 2.22.0 */
#if GTK_CHECK_VERSION(2,22,0)
#include <gdk/gdkkeysyms-compat.h>
#else
#include <gdk/gdkkeysyms.h>
#endif

#include "str_utils.h"
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
#include "logger.h"
#include "contacts/calltab.h"
#include "contacts/searchbar.h"
#include "contacts/addrbookfactory.h"
#include "icons/icon_factory.h"
#include "imwindow.h"
#include "statusicon.h"
#include "unused.h"
#include "widget/imwidget.h"
#include "sliders.h"

static GHashTable * ip2ip_profile;

void
sflphone_notify_voice_mail(const gchar* accountID , guint count)
{
    // We want to notify only the current account; ie the first in the list
    gchar *id = g_strdup(accountID);
    const gchar * const current_id = account_list_get_current_id();

    DEBUG("sflphone_notify_voice_mail begin");

    if (g_ascii_strcasecmp(id, current_id) != 0 ||
        account_list_get_size() == 0)
        return;

    // Set the number of voice messages for the current account
    current_account_set_message_number(count);
    account_t *current = account_list_get_current();

    // Update the voicemail tool button
    update_voicemail_status();

    if (current)
        notify_voice_mails(count, current);

    DEBUG("sflphone_notify_voice_mail end");
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

    gchar* msg;
    if (acc) {
        msg = g_markup_printf_escaped("%s %s (%s)" ,
                                      _("Using account"),
                                      (gchar*) account_lookup(acc, ACCOUNT_ALIAS),
                                      (gchar*) account_lookup(acc, ACCOUNT_TYPE));
    } else {
        msg = g_markup_printf_escaped(_("No registered accounts"));
    }

    statusbar_push_message(msg, NULL,  __MSG_ACCOUNT_DEFAULT);
    g_free(msg);
}


void
sflphone_quit()
{
    if (calllist_get_size(current_calls_tab) == 0 || main_window_ask_quit()) {
        dbus_unregister(getpid());
        dbus_clean();
        account_list_free();
        calllist_clean(current_calls_tab);
        calllist_clean(contacts_tab);
        calllist_clean(history_tab);
        gtk_main_quit();
    }
}

void
sflphone_hold(callable_obj_t * c)
{
    c->_state = CALL_STATE_HOLD;
    calltree_update_call(current_calls_tab, c);
    update_actions();
}

void
sflphone_ringing(callable_obj_t * c)
{
    c->_state = CALL_STATE_RINGING;
    calltree_update_call(current_calls_tab, c);
    update_actions();
}

void
sflphone_hung_up(callable_obj_t * c)
{
    DEBUG("%s", __PRETTY_FUNCTION__);

    calllist_remove_call(current_calls_tab, c->_callID);
    calltree_remove_call(current_calls_tab, c->_callID);
    c->_state = CALL_STATE_DIALING;
    call_remove_all_errors(c);
    update_actions();

    // test whether the widget contains text, if not remove it
    if ((im_window_get_nb_tabs() > 1) && c->_im_widget && !(IM_WIDGET(c->_im_widget)->containText))
        im_window_remove_tab(c->_im_widget);
    else
        im_widget_update_state(IM_WIDGET(c->_im_widget), FALSE);

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
            ERROR("SFLphone: Error: Could not fetch details for account %s",
                  accountID);
            break;
        }
        account_list_add(acc);
        /* Fill the actual array of credentials */
        dbus_get_credentials(acc);
        gchar * status = account_lookup(acc, ACCOUNT_REGISTRATION_STATUS);

        if (g_strcmp0(status, "REGISTERED") == 0)
            acc->state = ACCOUNT_STATE_REGISTERED;
        else if (g_strcmp0(status, "UNREGISTERED") == 0)
            acc->state = ACCOUNT_STATE_UNREGISTERED;
        else if (g_strcmp0(status, "TRYING") == 0)
            acc->state = ACCOUNT_STATE_TRYING;
        else if (g_strcmp0(status, "ERROR") == 0)
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
        else if (g_strcmp0(status, "ACCOUNT_STATE_IP2IP_READY") == 0)
            acc->state = ACCOUNT_STATE_IP2IP_READY;
        else
            acc->state = ACCOUNT_STATE_INVALID;

        gchar * code = account_lookup(acc, ACCOUNT_REGISTRATION_STATE_CODE);
        if (code != NULL)
            acc->protocol_state_code = atoi(code);
        acc->protocol_state_description = account_lookup(acc, ACCOUNT_REGISTRATION_STATE_DESC);
    }

    g_strfreev(array);

    // Set the current account message number
    current_account_set_message_number(current_account_get_message_number());
}

gboolean sflphone_init(GError **error)
{
    if (!dbus_connect(error) || !dbus_register(getpid(), "Gtk+ Client", error))
        return FALSE;

    abook_init();

    // Init icons factory
    init_icon_factory();

    current_calls_tab = calltab_init(FALSE, CURRENT_CALLS);
    contacts_tab = calltab_init(TRUE, CONTACTS);
    history_tab = calltab_init(TRUE, HISTORY);

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
    ip2ip_profile = (GHashTable *) dbus_get_ip2_ip_details();
}

GHashTable *sflphone_get_ip2ip_properties(void)
{
    return ip2ip_profile;
}

void
sflphone_hang_up()
{
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls_tab);
    conference_obj_t * selectedConf = calltab_get_selected_conf(active_calltree_tab);

    DEBUG("%s", __PRETTY_FUNCTION__);

    if (selectedConf) {
        im_widget_update_state(IM_WIDGET(selectedConf->_im_widget), FALSE);
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
            case CALL_STATE_RECORD:
                dbus_hang_up(selectedCall);
                call_remove_all_errors(selectedCall);
                selectedCall->_state = CALL_STATE_DIALING;
                time(&selectedCall->_time_stop);

                im_widget_update_state(IM_WIDGET(selectedCall->_im_widget), FALSE);

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
                DEBUG("from sflphone_hang_up : ");
                break;
            case CALL_STATE_TRANSFER:
                dbus_hang_up(selectedCall);
                call_remove_all_errors(selectedCall);
                time(&selectedCall->_time_stop);
                break;
            default:
                WARN("Should not happen in sflphone_hang_up()!");
                break;
        }
    }

    calltree_update_call(history_tab, selectedCall);

    statusbar_update_clock("");

    // Allow screen saver to start
    guint nbcall = calllist_get_size(current_calls_tab);
    if(nbcall == 1)
        dbus_screensaver_uninhibit();
}

void
sflphone_pick_up()
{
    callable_obj_t *selectedCall = calltab_get_selected_call(active_calltree_tab);

    // Disable screensaver if the list is empty call
    guint nbcall = calllist_get_size(current_calls_tab);
    if(nbcall == 0)
        dbus_screensaver_inhibit();

    if (!selectedCall) {
        sflphone_new_call();
        return;
    }

    switch (selectedCall->_state) {
        case CALL_STATE_DIALING:
            sflphone_place_call(selectedCall);

            // if instant messaging window is visible, create new tab (deleted automatically if not used)
            if (im_window_is_visible())
                if (!selectedCall->_im_widget)
                    selectedCall->_im_widget = im_widget_display(selectedCall->_callID);

            break;
        case CALL_STATE_INCOMING:
            selectedCall->_history_state = g_strdup(INCOMING_STRING);
            calltree_update_call(history_tab, selectedCall);

            // if instant messaging window is visible, create new tab (deleted automatically if not used)
            if (im_window_is_visible())
                if (!selectedCall->_im_widget)
                    selectedCall->_im_widget = im_widget_display(selectedCall->_callID);

            dbus_accept(selectedCall);
            break;
        case CALL_STATE_TRANSFER:
            dbus_transfer(selectedCall);
            time(&selectedCall->_time_stop);
            calltree_remove_call(current_calls_tab, selectedCall->_callID);
            update_actions();
            calllist_remove_call(current_calls_tab, selectedCall->_callID);
            break;
        case CALL_STATE_CURRENT:
        case CALL_STATE_HOLD:
        case CALL_STATE_RECORD:
        case CALL_STATE_RINGING:
            sflphone_new_call();
            break;
        default:
            WARN("Should not happen in sflphone_pick_up()!");
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
            case CALL_STATE_RECORD:
                dbus_hold(selectedCall);
                break;
            default:
                WARN("Should not happen in sflphone_on_hold!");
                break;
        }
    } else if (selectedConf)
        dbus_hold_conference(selectedConf);
}

void
sflphone_off_hold()
{
    DEBUG("%s", __PRETTY_FUNCTION__);
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls_tab);
    conference_obj_t * selectedConf = calltab_get_selected_conf(active_calltree_tab);

    if (selectedCall) {
        switch (selectedCall->_state) {
            case CALL_STATE_HOLD:
                dbus_unhold(selectedCall);
                break;
            default:
                WARN("Should not happen in sflphone_off_hold ()!");
                break;
        }
    } else if (selectedConf)
        dbus_unhold_conference(selectedConf);
}


void
sflphone_fail(callable_obj_t * c)
{
    c->_state = CALL_STATE_FAILURE;
    calltree_update_call(current_calls_tab, c);
    update_actions();
}

void
sflphone_busy(callable_obj_t * c)
{
    c->_state = CALL_STATE_BUSY;
    calltree_update_call(current_calls_tab, c);
    update_actions();
}

void
sflphone_current(callable_obj_t * c)
{
    if (c->_state != CALL_STATE_HOLD)
        time(&c->_time_start);

    c->_state = CALL_STATE_CURRENT;
    calltree_update_call(current_calls_tab, c);
    update_actions();
}

void
sflphone_record(callable_obj_t * c)
{
    if (c->_state != CALL_STATE_HOLD)
        time(&c->_time_start);

    c->_state = CALL_STATE_RECORD;
    calltree_update_call(current_calls_tab, c);
    update_actions();
}

void
sflphone_set_transfer()
{
    callable_obj_t * c = calltab_get_selected_call(current_calls_tab);

    if (c) {
        c->_state = CALL_STATE_TRANSFER;
        g_free(c->_trsft_to);
        c->_trsft_to = g_strdup("");
        calltree_update_call(current_calls_tab, c);
    }

    update_actions();
}

void
sflphone_unset_transfer()
{
    callable_obj_t * c = calltab_get_selected_call(current_calls_tab);

    if (c) {
        c->_state = CALL_STATE_CURRENT;
        g_free(c->_trsft_to);
        c->_trsft_to = g_strdup("");
        calltree_update_call(current_calls_tab, c);
    }

    update_actions();
}

void
sflphone_display_transfer_status(const gchar* message)
{
    statusbar_push_message(message , NULL, __MSG_ACCOUNT_DEFAULT);
}

void
sflphone_incoming_call(callable_obj_t * c)
{
    c->_history_state = g_strdup(MISSED_STRING);
    calllist_add_call(current_calls_tab, c);
    calltree_add_call(current_calls_tab, c, NULL);

    update_actions();
    calltree_display(current_calls_tab);

    // Change the status bar if we are dealing with a direct SIP call
    if (is_direct_call(c)) {
        gchar *msg = g_markup_printf_escaped(_("Direct SIP call"));
        statusbar_pop_message(__MSG_ACCOUNT_DEFAULT);
        statusbar_push_message(msg , NULL, __MSG_ACCOUNT_DEFAULT);
        g_free(msg);
    }
}

static void
process_dialing(callable_obj_t *c, guint keyval, gchar *key)
{
    // We stop the tone
    if (!*c->_peer_number && c->_state != CALL_STATE_TRANSFER)
        dbus_start_tone(FALSE, 0);

    switch (keyval) {
        case GDK_Return:
        case GDK_KP_Enter:
            sflphone_place_call(c);
            break;
        case GDK_Escape:
            sflphone_hang_up();
            break;
        case GDK_BackSpace: {
            gchar *num = (c->_state == CALL_STATE_TRANSFER) ? c->_trsft_to : c->_peer_number;
            size_t len = strlen(num);

            if (len) {
                len--; // delete one character
                num[len] = '\0';
                calltree_update_call(current_calls_tab, c);

                /* If number is now empty, hang up immediately */
                if (c->_state != CALL_STATE_TRANSFER && len == 0)
                    dbus_hang_up(c);
            }

            break;
        }
        case GDK_Tab:
        case GDK_Alt_L:
        case GDK_Control_L:
        case GDK_Super_L:
        case GDK_Caps_Lock:
            break;
        default:

            if (keyval < 127 /* ascii */ ||
                    (keyval >= GDK_Mode_switch && keyval <= GDK_KP_9) /* num keypad */) {

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

                calltree_update_call(current_calls_tab, c);
            }

            break;
    }
}


callable_obj_t *
sflphone_new_call()
{
    // Disable screensaver if the list is empty call
    guint nbcall = calllist_get_size(current_calls_tab);
    if(nbcall == 0)
        dbus_screensaver_inhibit();

    callable_obj_t *current_selected_call = calltab_get_selected_call(current_calls_tab);

    if (current_selected_call != NULL) {
        gchar *confID = dbus_get_conference_id(current_selected_call->_callID);
        if(g_strcmp0(confID, "") != 0) {
            sflphone_on_hold();
        }
    }

    // Play a tone when creating a new call
    if (calllist_get_size(current_calls_tab) == 0)
        dbus_start_tone(TRUE , (current_account_has_new_message()  > 0) ? TONE_WITH_MESSAGE : TONE_WITHOUT_MESSAGE) ;

    callable_obj_t *c = create_new_call(CALL, CALL_STATE_DIALING, "", "", "", "");

    c->_history_state = g_strdup(OUTGOING_STRING);

    calllist_add_call(current_calls_tab, c);
    calltree_add_call(current_calls_tab, c, NULL);
    update_actions();

    return c;
}


void
sflphone_keypad(guint keyval, gchar * key)
{
    callable_obj_t * c = calltab_get_selected_call(current_calls_tab);

    if ((active_calltree_tab != current_calls_tab) || (active_calltree_tab == current_calls_tab && !c)) {
        switch (keyval) {
            case GDK_Return:
            case GDK_KP_Enter:
            case GDK_Escape:
            case GDK_BackSpace:
                break;
            default:
                calltree_display(current_calls_tab);
                process_dialing(sflphone_new_call(), keyval, key);
                break;
        }
    } else if (c) {
        switch (c->_state) {
            case CALL_STATE_DIALING: // Currently dialing => edit number
                process_dialing(c, keyval, key);
                break;
            case CALL_STATE_RECORD:
            case CALL_STATE_CURRENT:

                switch (keyval) {
                    case GDK_Escape:
                        dbus_hang_up(c);
                        time(&c->_time_stop);
                        calltree_update_call(history_tab, c);
                        break;
                    default:
                        // To play the dtmf when calling mail box for instance
                        dbus_play_dtmf(key);
                        break;
                }

                break;
            case CALL_STATE_INCOMING:

                switch (keyval) {
                    case GDK_Return:
                    case GDK_KP_Enter:
                        c->_history_state = g_strdup(INCOMING_STRING);
                        calltree_update_call(history_tab, c);
                        dbus_accept(c);
                        break;
                    case GDK_Escape:
                        dbus_refuse(c);
                        break;
                }

                break;
            case CALL_STATE_TRANSFER:

                switch (keyval) {
                    case GDK_Return:
                    case GDK_KP_Enter:
                        dbus_transfer(c);
                        time(&c->_time_stop);
                        calltree_remove_call(current_calls_tab, c->_callID);
                        update_actions();
                        break;
                    case GDK_Escape:
                        sflphone_unset_transfer();
                        break;
                    default: // When a call is on transfer, typing new numbers will add it to c->_peer_number
                        process_dialing(c, keyval, key);
                        break;
                }

                break;
            case CALL_STATE_HOLD:

                switch (keyval) {
                    case GDK_Return:
                    case GDK_KP_Enter:
                        dbus_unhold(c);
                        break;
                    case GDK_Escape:
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

                switch (keyval) {
                    case GDK_Escape:
                        dbus_hang_up(c);
                        calltree_update_call(history_tab, c);
                        break;
                }

                break;
            default:
                break;
        }

    } else
        sflphone_new_call();
}

int
sflphone_place_call(callable_obj_t * c)
{
    account_t * account = NULL;

    if (c == NULL) {
        ERROR("Callable object is NULL while making new call");
        return -1;
    }

    DEBUG("Placing call from %s to %s using account %s", c->_display_name, c->_peer_number, c->_accountID);

    if (c->_state != CALL_STATE_DIALING) {
        ERROR("Call not in state dialing, cannot place call");
        return -1;
    }

    if (!c->_peer_number || strlen(c->_peer_number) == 0) {
        ERROR("No peer number set for this call");
        return -1;
    }

    // Get the account for this call
    if (strlen(c->_accountID) != 0) {
        DEBUG("Account %s already set for this call", c->_accountID);
        account = account_list_get_by_id(c->_accountID);
    } else {
        DEBUG("No account set for this call, use first of the list");
        account = account_list_get_current();
    }

    // Make sure the previously found account is registered, take first one registered elsewhere
    if (account) {
        gpointer status = g_hash_table_lookup(account->properties, "Status");
        if (!utf8_case_equal(status, "REGISTERED")) {
            // Place the call with the first registered account
            account = account_list_get_by_state(ACCOUNT_STATE_REGISTERED);
        }
    }

    // If there is no account specified or found, fallback on IP2IP call
    if(account == NULL) {
        DEBUG("Could not find an account for this call, making ip to ip call");
        account = account_list_get_by_id("IP2IP");
        if (account == NULL) {
            ERROR("Actions: Could not determine any account for this call");
            return -1;
        }
    }

    // free memory for previous account id and use the new one in case it changed
    g_free(c->_accountID);
    c->_accountID = g_strdup(account->accountID);
    dbus_place_call(c);
    notify_current_account(account);

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

    DEBUG("Detach participant %s", selectedCall->_callID);

    im_widget_update_state(IM_WIDGET(selectedCall->_im_widget), TRUE);
    calltree_remove_call(current_calls_tab, selectedCall->_callID);
    calltree_add_call(current_calls_tab, selectedCall, NULL);
    dbus_detach_participant(selectedCall->_callID);
}

void
sflphone_add_participant(const gchar* callID, const gchar* confID)
{
    DEBUG("Add participant %s to conference %s", callID, confID);

    callable_obj_t *call = calllist_get_call(current_calls_tab, callID);

    if (call == NULL) {
        ERROR("Could not find call");
        return;
    }

    dbus_add_participant(callID, confID);
}

void
sflphone_add_main_participant(const conference_obj_t * c)
{
    DEBUG("%s", __PRETTY_FUNCTION__);
    dbus_add_main_participant(c->_confID);
}

void
sflphone_rec_call()
{
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls_tab);
    conference_obj_t * selectedConf = calltab_get_selected_conf(current_calls_tab);

    if (selectedCall) {
        DEBUG("Set record for selected call");
        dbus_set_record(selectedCall->_callID);

        switch (selectedCall->_state) {
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

        calltree_update_call(current_calls_tab, selectedCall);
    } else if (selectedConf) {
        DEBUG("Set record for selected conf");
        dbus_set_record(selectedConf->_confID);

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
                WARN("Should not happen in sflphone_off_hold ()!");
                break;
        }

        DEBUG("Remove and add conference %s", selectedConf->_confID);
        calltree_remove_conference(current_calls_tab, selectedConf);
        calltree_add_conference_to_current_calls(selectedConf);
    }

    update_actions();
}

void
sflphone_mute_call()
{
    DEBUG("%s", __PRETTY_FUNCTION__);

    toggle_slider_mute_microphone();
}

#ifdef SFL_VIDEO
static void
sflphone_fill_video_codec_list_per_account(account_t *account)
{
    if (!account->vcodecs)
        account->vcodecs = g_queue_new();
    else
        g_queue_clear(account->vcodecs);

    /* First add the active codecs for this account */
    GQueue* system_vcodecs = get_video_codecs_list();
    gchar **order = dbus_get_active_video_codec_list(account->accountID);
    for (gchar **pl = order; *pl; pl++) {
        codec_t *orig = codec_list_get_by_name(*pl, system_vcodecs);
        codec_t *c = codec_create_new_from_caps(orig);
        if (c)
            g_queue_push_tail(account->vcodecs, c);
        else
            ERROR("Couldn't find codec %s %p", *pl, orig);
        g_free(*pl);
    }
    g_free(order);

    /* Here we add installed codecs that aren't active for the account */
    guint caps_size = g_queue_get_length(system_vcodecs);
    for (guint i = 0; i < caps_size; ++i) {
        codec_t * vcodec = g_queue_peek_nth(system_vcodecs, i);
        if (codec_list_get_by_name(vcodec->name, account->vcodecs) == NULL) {
            vcodec->is_active = FALSE;
            g_queue_push_tail(account->vcodecs, vcodec);
        }
    }
}
#endif

static void
sflphone_fill_audio_codec_list_per_account(account_t *account)
{
    if (!account->acodecs)
        account->acodecs = g_queue_new();
    else
        g_queue_clear(account->acodecs);

    /* First add the active codecs for this account */
    GArray *order = dbus_get_active_audio_codec_list(account->accountID);
    GQueue *system_acodecs = get_audio_codecs_list();
    for (guint i = 0; i < order->len; i++) {
        gint payload = g_array_index(order, gint, i);
        codec_t *orig = codec_list_get_by_payload(payload, system_acodecs);
        codec_t *c = codec_create_new_from_caps(orig);

        if (c)
            g_queue_push_tail(account->acodecs, c);
        else
            ERROR("Couldn't find codec %d %p", payload, orig);
    }
    g_array_unref(order);

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
#ifdef SFL_VIDEO
    sflphone_fill_video_codec_list_per_account(account);
#endif
}

void sflphone_fill_call_list(void)
{
    gchar **call_list = dbus_get_call_list();

    for (gchar **callp = call_list; callp && *callp; ++callp) {
        gchar *callID = *callp;
        if (!calllist_get_call(current_calls_tab, callID)) {
            callable_obj_t *call = create_new_call_from_details(*callp, dbus_get_call_details(*callp));
            call->_zrtp_confirmed = FALSE;
            calllist_add_call(current_calls_tab, call);

            // add in treeview only if does not participate to a conference
            gchar *confID = dbus_get_conference_id(call->_callID);
            if(g_strcmp0(confID, "") == 0)
                calltree_add_call(current_calls_tab, call, NULL);
        }
    }

    g_strfreev(call_list);
}


void sflphone_fill_conference_list(void)
{
    // TODO Fetch the active conferences at client startup

    gchar **conferences = dbus_get_conference_list();

    for (gchar **list = conferences; list && *list; list++) {
        const gchar * const conf_id = *list;

        GHashTable *conference_details = dbus_get_conference_details(conf_id);
        conference_obj_t *conf = create_new_conference_from_details(conf_id, conference_details);

        conferencelist_add(current_calls_tab, conf);
        calltree_add_conference_to_current_calls(conf);
    }

    g_strfreev(conferences);
}

static void
create_callable_from_entry(gpointer data, gpointer user_data UNUSED)
{
    GHashTable *entry = (GHashTable *) data;
    callable_obj_t *history_call = create_history_entry_from_hashtable(entry);

    /* Add it and update the GUI */
    calllist_add_call_to_front(history_tab, history_call);
}

static void fill_treeview_with_calls(void)
{
    guint n = calllist_get_size(history_tab);

    for (guint i = 0; i < n; ++i) {
        callable_obj_t *call = calllist_get_nth(history_tab, i);
        if (call)
            calltree_add_history_entry(call);
    }
}

void sflphone_fill_history(void)
{
    GPtrArray *entries = dbus_get_history();
    if (entries)
        g_ptr_array_foreach(entries, create_callable_from_entry, NULL);

    fill_treeview_with_calls();
}

void
sflphone_srtp_sdes_on(callable_obj_t * c)
{
    c->_srtp_state = SRTP_STATE_SDES_SUCCESS;

    calltree_update_call(current_calls_tab, c);
    update_actions();
}

void
sflphone_srtp_sdes_off(callable_obj_t * c)
{
    c->_srtp_state = SRTP_STATE_UNLOCKED;

    calltree_update_call(current_calls_tab, c);
    update_actions();
}


void
sflphone_srtp_zrtp_on(callable_obj_t * c)
{
    c->_srtp_state = SRTP_STATE_ZRTP_SAS_UNCONFIRMED;

    calltree_update_call(current_calls_tab, c);
    update_actions();
}

void
sflphone_srtp_zrtp_off(callable_obj_t * c)
{
    c->_srtp_state = SRTP_STATE_UNLOCKED;
    calltree_update_call(current_calls_tab, c);
    update_actions();
}

void
sflphone_srtp_zrtp_show_sas(callable_obj_t * c, const gchar* sas, const gboolean verified)
{
    c->_sas = g_strdup(sas);
    c->_srtp_state = verified ? SRTP_STATE_ZRTP_SAS_CONFIRMED : SRTP_STATE_ZRTP_SAS_UNCONFIRMED;

    calltree_update_call(current_calls_tab, c);
    update_actions();
}

void
sflphone_request_go_clear(void)
{
    callable_obj_t * selectedCall = calltab_get_selected_call(current_calls_tab);

    if (selectedCall)
        dbus_request_go_clear(selectedCall);
}

void
sflphone_call_state_changed(callable_obj_t * c, const gchar * description, const guint code)
{
    DEBUG("Call State changed %s", description);

    if (c == NULL) {
        ERROR("SFLphone: Error: callable obj is NULL in %s at %d", __FILE__, __LINE__);
        return;
    }

    g_free(c->_state_code_description);
    c->_state_code_description = g_strdup(description);
    c->_state_code = code;

    calltree_update_call(current_calls_tab, c);
    update_actions();
}

