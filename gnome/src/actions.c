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

#include <gtk/gtk.h>
/* Backward compatibility for gtk < 2.22.0 */
#if GTK_CHECK_VERSION(2,22,0)
#include <gdk/gdkkeysyms-compat.h>
#else
#include <gdk/gdkkeysyms.h>
#endif
#include <glib/gprintf.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>

#include <arpa/nameser.h>
#include <netinet/in.h>
#include <resolv.h>

#include <linux/if.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

#include "actions.h"
#include "dbus/dbus.h"
#include "contacts/searchbar.h"
#include "contacts/addrbookfactory.h"
#include "icons/icon_factory.h"
#include "imwindow.h"
#include "statusicon.h"
#include "widget/imwidget.h"


static GHashTable * ip2ip_profile;

static gchar ** sflphone_order_history_hash_table(GHashTable *result)
{
    GHashTableIter iter;
    gint size = 0;
    gchar **ordered_list = NULL;

    assert(result);
    while (g_hash_table_size (result)) {
        gpointer key, key_to_min, value;

        // find lowest timestamp in map
        g_hash_table_iter_init (&iter, result);

        gint min_timestamp = G_MAXINT;
        while (g_hash_table_iter_next (&iter, &key, &value))  {
            gint timestamp = atoi ( (gchar*) key);
            if (timestamp < min_timestamp) {
                min_timestamp = timestamp;
                key_to_min = key;
            }
        }

        if (g_hash_table_lookup_extended(result, key_to_min, &key, &value)) {
            GSList *llist = (GSList *)value;
            while (llist) {
                ordered_list = (void *) g_realloc(ordered_list, (size + 1) * sizeof (void *));
                *(ordered_list + size) = g_strdup((gchar *)llist->data);
                size++;
                llist = g_slist_next(llist);
            }
            g_hash_table_remove(result, key_to_min);
        }
    }

    ordered_list = (void *) g_realloc(ordered_list, (size + 1) * sizeof(void *));
    *(ordered_list + size) = NULL;

    return ordered_list;
}

void
sflphone_notify_voice_mail (const gchar* accountID , guint count)
{
    // We want to notify only the current account; ie the first in the list
    gchar *id = g_strdup (accountID);
    const gchar * const current_id = account_list_get_current_id ();

    DEBUG ("sflphone_notify_voice_mail begin");

    if (g_ascii_strcasecmp (id, current_id) != 0 || account_list_get_size() == 0)
        return;

    // Set the number of voice messages for the current account
    current_account_set_message_number (count);
    account_t *current = account_list_get_current ();

    // Update the voicemail tool button
    update_voicemail_status ();

    if (current)
        notify_voice_mails (count, current);

    DEBUG ("sflphone_notify_voice_mail end");
}

/*
 * Place a call with the current account.
 * If there is no default account selected, place a call with the first
 * registered account of the account list
 * Else, check if it an IP call. if not, popup an error message
 */

static gboolean _is_direct_call (callable_obj_t * c)
{
    if (g_strcasecmp (c->_accountID, "empty") == 0) {
        if (!g_str_has_prefix (c->_peer_number, "sip:")) {
            gchar * new_number = g_strconcat ("sip:", c->_peer_number, NULL);
            g_free (c->_peer_number);
            c->_peer_number = new_number;
        }

        return TRUE;
    }

    return g_str_has_prefix (c->_peer_number, "sip:") ||
           g_str_has_prefix (c->_peer_number, "sips:");
}


void
status_bar_display_account ()
{
    gchar* msg;

    statusbar_pop_message (__MSG_ACCOUNT_DEFAULT);

    account_t *acc = account_list_get_current ();
    status_tray_icon_online (acc != NULL);

    if (acc) {
        msg = g_markup_printf_escaped ("%s %s (%s)" ,
                                       _ ("Using account"),
                                       (gchar*) g_hash_table_lookup (acc->properties , ACCOUNT_ALIAS),
                                       (gchar*) g_hash_table_lookup (acc->properties , ACCOUNT_TYPE));
    } else {
        msg = g_markup_printf_escaped (_ ("No registered accounts"));
    }

    statusbar_push_message (msg, NULL,  __MSG_ACCOUNT_DEFAULT);
    g_free (msg);
}


void
sflphone_quit ()
{
    if (calllist_get_size(current_calls) == 0 || main_window_ask_quit()) {
        // Save the history
        sflphone_save_history ();

        dbus_unregister (getpid());
        dbus_clean ();
        calllist_clean (current_calls);
        calllist_clean (contacts);
        calllist_clean (history);
        gtk_main_quit ();
    }
}

void
sflphone_hold (callable_obj_t * c)
{
    c->_state = CALL_STATE_HOLD;
    calltree_update_call (current_calls, c, NULL);
    update_actions();
}

void
sflphone_ringing (callable_obj_t * c)
{
    c->_state = CALL_STATE_RINGING;
    calltree_update_call (current_calls, c, NULL);
    update_actions();
}

void
sflphone_hung_up (callable_obj_t * c)
{
    DEBUG ("SFLphone: Hung up");

    calllist_remove_call (current_calls, c->_callID);
    calltree_remove_call (current_calls, c, NULL);
    c->_state = CALL_STATE_DIALING;
    call_remove_all_errors (c);
    update_actions();

    if(c->_confID) {
	g_free(c->_confID);
	c->_confID = NULL;
    }

    // test wether the widget contain text, if not remove it
    if ( (im_window_get_nb_tabs() > 1) && c->_im_widget && ! (IM_WIDGET (c->_im_widget)->containText))
        im_window_remove_tab (c->_im_widget);
    else
        im_widget_update_state (IM_WIDGET (c->_im_widget), FALSE);

#if GTK_CHECK_VERSION(2,10,0)
    status_tray_icon_blink (FALSE);
#endif

    statusbar_update_clock("");
}

/** Internal to actions: Fill account list */
void sflphone_fill_account_list (void)
{

    gchar** array;
    gchar** accountID;
    unsigned int i;
    int count;

    DEBUG ("SFLphone: Fill account list");

    count = current_account_get_message_number ();

    account_list_clear ();

    array = (gchar **) dbus_account_list();

    if (array) {

        for (accountID = array; *accountID; accountID++) {
            account_t * a = g_new0 (account_t,1);
            a->accountID = g_strdup (*accountID);
            a->credential_information = NULL;
            account_list_add (a);
        }

        g_strfreev (array);
    }

    for (i = 0; i < account_list_get_size(); i++) {
        account_t  * a = account_list_get_nth (i);
        if(a == NULL) {
            ERROR("SFLphone: Error: Could not find account %d in list", i);
            break;
        }

        GHashTable * details = (GHashTable *) dbus_get_account_details (a->accountID);
        if (details == NULL) {
            ERROR("SFLphone: Error: Could not fetch detais for account %s", a->accountID);
	    break;
        }

        a->properties = details;

        /* Fill the actual array of credentials */
        dbus_get_credentials (a);

        gchar * status = g_hash_table_lookup (details, REGISTRATION_STATUS);

        if (g_strcmp0 (status, "REGISTERED") == 0) {
            a->state = ACCOUNT_STATE_REGISTERED;
        } else if (g_strcmp0 (status, "UNREGISTERED") == 0) {
            a->state = ACCOUNT_STATE_UNREGISTERED;
        } else if (g_strcmp0 (status, "TRYING") == 0) {
            a->state = ACCOUNT_STATE_TRYING;
        } else if (g_strcmp0 (status, "ERROR") == 0) {
            a->state = ACCOUNT_STATE_ERROR;
        } else if (g_strcmp0 (status , "ERROR_AUTH") == 0) {
            a->state = ACCOUNT_STATE_ERROR_AUTH;
        } else if (g_strcmp0 (status , "ERROR_NETWORK") == 0) {
            a->state = ACCOUNT_STATE_ERROR_NETWORK;
        } else if (g_strcmp0 (status , "ERROR_HOST") == 0) {
            a->state = ACCOUNT_STATE_ERROR_HOST;
        } else if (g_strcmp0 (status , "ERROR_CONF_STUN") == 0) {
            a->state = ACCOUNT_STATE_ERROR_CONF_STUN;
        } else if (g_strcmp0 (status , "ERROR_EXIST_STUN") == 0) {
            a->state = ACCOUNT_STATE_ERROR_EXIST_STUN;
        } else if (g_strcmp0 (status, "READY") == 0) {
            a->state = IP2IP_PROFILE_STATUS;
        } else {
            a->state = ACCOUNT_STATE_INVALID;
        }

        gchar * code = NULL;
        code = g_hash_table_lookup (details, REGISTRATION_STATE_CODE);

        if (code != NULL) {
            a->protocol_state_code = atoi (code);
        }

        g_free (a->protocol_state_description);
        a->protocol_state_description = g_hash_table_lookup (details, REGISTRATION_STATE_DESCRIPTION);
    }

    // Set the current account message number
    current_account_set_message_number (count);

    sflphone_fill_codec_list ();
}

gboolean sflphone_init (GError **error)
{
    if (!dbus_connect (error) || !dbus_register (getpid (), "Gtk+ Client", error))
        return FALSE;

    abook_init();

    // Init icons factory
    init_icon_factory ();

    current_calls = calltab_init (FALSE, CURRENT_CALLS);
    contacts = calltab_init (TRUE, CONTACTS);
    history = calltab_init (TRUE, HISTORY);

    account_list_init ();
    codec_capabilities_load ();
    conferencelist_init (current_calls);
    conferencelist_init (history);

    // Fetch the configured accounts
    sflphone_fill_account_list ();

    // Fetch the ip2ip profile
    sflphone_fill_ip2ip_profile();

    // Fetch the conference list
    // sflphone_fill_conference_list();

    return TRUE;
}

void sflphone_fill_ip2ip_profile (void)
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
    callable_obj_t * selectedCall = calltab_get_selected_call (current_calls);
    conference_obj_t * selectedConf = calltab_get_selected_conf (active_calltree);

    DEBUG ("SFLphone: Hang up");

    if (selectedCall) {
        switch (selectedCall->_state) {
            case CALL_STATE_DIALING:
                dbus_hang_up (selectedCall);
                break;
            case CALL_STATE_RINGING:
                dbus_hang_up (selectedCall);
                call_remove_all_errors (selectedCall);
                selectedCall->_state = CALL_STATE_DIALING;
                //selectedCall->_stop = 0;
                break;
            case CALL_STATE_CURRENT:
            case CALL_STATE_HOLD:
            case CALL_STATE_BUSY:
            case CALL_STATE_RECORD:
                dbus_hang_up (selectedCall);
                call_remove_all_errors (selectedCall);
                selectedCall->_state = CALL_STATE_DIALING;
                time (&selectedCall->_time_stop);

                //if ( (im_window_get_nb_tabs() > 1) && selectedCall->_im_widget &&
                //        ! (IM_WIDGET (selectedCall->_im_widget)->containText))
                //    im_window_remove_tab (selectedCall->_im_widget);
                //else
                im_widget_update_state (IM_WIDGET (selectedCall->_im_widget), FALSE);

                break;
            case CALL_STATE_FAILURE:
                dbus_hang_up (selectedCall);
                call_remove_all_errors (selectedCall);
                selectedCall->_state = CALL_STATE_DIALING;
                break;
            case CALL_STATE_INCOMING:
                dbus_refuse (selectedCall);
                call_remove_all_errors (selectedCall);
                selectedCall->_state = CALL_STATE_DIALING;
                DEBUG ("from sflphone_hang_up : ");
                break;
            case CALL_STATE_TRANSFER:
                dbus_hang_up (selectedCall);
                call_remove_all_errors (selectedCall);
                time (&selectedCall->_time_stop);
                break;
            default:
                WARN ("Should not happen in sflphone_hang_up()!");
                break;
        }
    } else if (selectedConf) {
        im_widget_update_state (IM_WIDGET (selectedConf->_im_widget), FALSE);
        dbus_hang_up_conference (selectedConf);
    }

    calltree_update_call (history, selectedCall, NULL);

    statusbar_update_clock("");
}

void
sflphone_pick_up()
{
    callable_obj_t *selectedCall = calltab_get_selected_call (active_calltree);

    DEBUG("SFLphone: Pick up");

    if (!selectedCall) {
        sflphone_new_call();
        return;
    }
    switch (selectedCall->_state) {
        case CALL_STATE_DIALING:
            sflphone_place_call (selectedCall);

            // if instant messaging window is visible, create new tab (deleted automatically if not used)
            if (im_window_is_visible())
                im_widget_display ( (IMWidget **) (&selectedCall->_im_widget), NULL, selectedCall->_callID, NULL);

            break;
        case CALL_STATE_INCOMING:
            selectedCall->_history_state = INCOMING;
            calltree_update_call (history, selectedCall, NULL);

            // if instant messaging window is visible, create new tab (deleted automatically if not used)
            if (selectedCall->_im_widget && im_window_is_visible()) {
                im_widget_display ( (IMWidget **) (&selectedCall->_im_widget), NULL, selectedCall->_callID, NULL);
            }

            dbus_accept (selectedCall);
            break;
        case CALL_STATE_TRANSFER:
            dbus_transfer (selectedCall);
            time (&selectedCall->_time_stop);
            calltree_remove_call(current_calls, selectedCall, NULL);
            calllist_remove_call(current_calls, selectedCall->_callID);
            break;
        case CALL_STATE_CURRENT:
        case CALL_STATE_HOLD:
        case CALL_STATE_RECORD:
        case CALL_STATE_RINGING:
            sflphone_new_call();
            break;
        default:
            WARN ("Should not happen in sflphone_pick_up()!");
            break;
    }
}

void
sflphone_on_hold ()
{
    callable_obj_t * selectedCall = calltab_get_selected_call (current_calls);
    conference_obj_t * selectedConf = calltab_get_selected_conf (active_calltree);

    DEBUG ("sflphone_on_hold");

    if (selectedCall) {
        switch (selectedCall->_state) {
            case CALL_STATE_CURRENT:
                dbus_hold (selectedCall);
                break;
            case CALL_STATE_RECORD:
                dbus_hold (selectedCall);
                break;
            default:
                WARN ("Should not happen in sflphone_on_hold!");
                break;
        }
    } else if (selectedConf) {
        dbus_hold_conference (selectedConf);
    }
}

void
sflphone_off_hold ()
{
    DEBUG ("sflphone_off_hold");
    callable_obj_t * selectedCall = calltab_get_selected_call (current_calls);
    conference_obj_t * selectedConf = calltab_get_selected_conf (active_calltree);

    if (selectedCall) {
        switch (selectedCall->_state) {
            case CALL_STATE_HOLD:
                dbus_unhold (selectedCall);
                break;
            default:
                WARN ("Should not happen in sflphone_off_hold ()!");
                break;
        }
    } else if (selectedConf) {


        dbus_unhold_conference (selectedConf);
    }
}


void
sflphone_fail (callable_obj_t * c)
{
    c->_state = CALL_STATE_FAILURE;
    calltree_update_call (current_calls, c, NULL);
    update_actions();
}

void
sflphone_busy (callable_obj_t * c)
{
    c->_state = CALL_STATE_BUSY;
    calltree_update_call (current_calls, c, NULL);
    update_actions();
}

void
sflphone_current (callable_obj_t * c)
{

    if (c->_state != CALL_STATE_HOLD)
        time (&c->_time_start);

    c->_state = CALL_STATE_CURRENT;
    calltree_update_call (current_calls, c, NULL);
    update_actions();
}

void
sflphone_record (callable_obj_t * c)
{
    if (c->_state != CALL_STATE_HOLD)
        time (&c->_time_start);

    c->_state = CALL_STATE_RECORD;
    calltree_update_call (current_calls, c, NULL);
    update_actions();
}

void
sflphone_set_transfer()
{
    callable_obj_t * c = calltab_get_selected_call (current_calls);

    if (c) {
        c->_state = CALL_STATE_TRANSFER;
        g_free(c->_trsft_to);
        c->_trsft_to = g_strdup ("");
        calltree_update_call (current_calls, c, NULL);
    }

    update_actions();
}

void
sflphone_unset_transfer()
{
    callable_obj_t * c = calltab_get_selected_call (current_calls);

    if (c) {
        c->_state = CALL_STATE_CURRENT;
        g_free(c->_trsft_to);
        c->_trsft_to = g_strdup ("");
        calltree_update_call (current_calls, c, NULL);
    }

    update_actions();
}

void
sflphone_display_transfer_status (const gchar* message)
{
    statusbar_push_message (message , NULL, __MSG_ACCOUNT_DEFAULT);
}

void
sflphone_incoming_call (callable_obj_t * c)
{
    c->_history_state = MISSED;
    calllist_add_call (current_calls, c);
    calltree_add_call (current_calls, c, NULL);

    update_actions();
    calltree_display (current_calls);

    // Change the status bar if we are dealing with a direct SIP call
    if (_is_direct_call (c)) {
        gchar *msg = g_markup_printf_escaped (_ ("Direct SIP call"));
        statusbar_pop_message (__MSG_ACCOUNT_DEFAULT);
        statusbar_push_message (msg , NULL, __MSG_ACCOUNT_DEFAULT);
        g_free (msg);
    }
}

/* Truncates last char from dynamically allocated string */
static void truncate_last_char(gchar **str)
{
    if (strlen(*str) > 0) {
        gchar *tmp = *str;
        tmp = g_strndup(*str, strlen(*str) - 1);
        g_free(*str);
        *str = tmp;
    }
}

void
process_dialing (callable_obj_t *c, guint keyval, gchar *key)
{
    // We stop the tone
    if (!*c->_peer_number && c->_state != CALL_STATE_TRANSFER)
        dbus_start_tone (FALSE, 0);

    switch (keyval) {
        case GDK_Return:
        case GDK_KP_Enter:
            sflphone_place_call (c);
            break;
        case GDK_Escape:
            sflphone_hang_up ();
            break;
        case GDK_BackSpace:
            if (c->_state == CALL_STATE_TRANSFER) {
                truncate_last_char(&c->_trsft_to);
                calltree_update_call (current_calls, c, NULL);
            } else {
                truncate_last_char(&c->_peer_number);
                calltree_update_call (current_calls, c, NULL);
                /* If number is now empty, hang up immediately */
                if (strlen(c->_peer_number) == 0)
                    dbus_hang_up(c);
            }

            break;
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
                    gchar *new_trsft = g_strconcat (c->_trsft_to, key, NULL);
                    g_free (c->_trsft_to);
                    c->_trsft_to = new_trsft;
                } else {
                    dbus_play_dtmf (key);
                    gchar *new_peer_number = g_strconcat (c->_peer_number, key, NULL);
                    g_free (c->_peer_number);
                    c->_peer_number = new_peer_number;
                }

                calltree_update_call (current_calls, c, NULL);
            }

            break;
    }
}


callable_obj_t *
sflphone_new_call()
{
    callable_obj_t *current_selected_call = calltab_get_selected_call (current_calls);

    if ( (current_selected_call != NULL) && (current_selected_call->_confID == NULL))
        sflphone_on_hold();

    // Play a tone when creating a new call
    if (calllist_get_size (current_calls) == 0)
        dbus_start_tone (TRUE , (current_account_has_new_message ()  > 0) ? TONE_WITH_MESSAGE : TONE_WITHOUT_MESSAGE) ;

    callable_obj_t *c = create_new_call (CALL, CALL_STATE_DIALING, "", "", "", "");

    c->_history_state = OUTGOING;

    calllist_add_call (current_calls, c);
    calltree_add_call (current_calls, c, NULL);
    update_actions();

    return c;
}


void
sflphone_keypad (guint keyval, gchar * key)
{
    callable_obj_t * c = calltab_get_selected_call (current_calls);

    if ( (active_calltree != current_calls) || (active_calltree == current_calls && !c)) {
        switch (keyval) {
            case GDK_Return:
            case GDK_KP_Enter:
            case GDK_Escape:
                break;
            default:
                calltree_display (current_calls);
                process_dialing (sflphone_new_call(), keyval, key);
                break;
        }
    } else if (c) {
        switch (c->_state) {
            case CALL_STATE_DIALING: // Currently dialing => edit number
                process_dialing (c, keyval, key);
                break;
            case CALL_STATE_RECORD:
            case CALL_STATE_CURRENT:

                switch (keyval) {
                    case GDK_Escape:
                        dbus_hang_up (c);
                        time (&c->_time_stop);
                        calltree_update_call (history, c, NULL);
                        break;
                    default:
                        // To play the dtmf when calling mail box for instance
                        dbus_play_dtmf (key);
                        break;
                }

                break;
            case CALL_STATE_INCOMING:

                switch (keyval) {
                    case GDK_Return:
                    case GDK_KP_Enter:
                        c->_history_state = INCOMING;
                        calltree_update_call (history, c, NULL);
                        dbus_accept (c);
                        break;
                    case GDK_Escape:
                        dbus_refuse (c);
                        break;
                }

                break;
            case CALL_STATE_TRANSFER:

                switch (keyval) {
                    case GDK_Return:
                    case GDK_KP_Enter:
                        dbus_transfer (c);
                        time (&c->_time_stop);
                        calltree_remove_call(current_calls, c, NULL);
                        break;
                    case GDK_Escape:
                        sflphone_unset_transfer ();
                        break;
                    default: // When a call is on transfer, typing new numbers will add it to c->_peer_number
                        process_dialing (c, keyval, key);
                        break;
                }

                break;
            case CALL_STATE_HOLD:

                switch (keyval) {
                    case GDK_Return:
                    case GDK_KP_Enter:
                        dbus_unhold (c);
                        break;
                    case GDK_Escape:
                        dbus_hang_up (c);
                        break;
                    default: // When a call is on hold, typing new numbers will create a new call
                        process_dialing (sflphone_new_call(), keyval, key);
                        break;
                }

                break;
            case CALL_STATE_RINGING:
            case CALL_STATE_BUSY:
            case CALL_STATE_FAILURE:

                switch (keyval) {
                    case GDK_Escape:
                        dbus_hang_up (c);
                        calltree_update_call (history, c, NULL);
                        break;
                }

                break;
            default:
                break;
        }

    } else
        sflphone_new_call();
}

static void place_direct_call (const callable_obj_t * c)
{
    g_assert(c->_state == CALL_STATE_DIALING);
    dbus_place_call(c);
}

static int place_registered_call (callable_obj_t * c)
{
    account_t * current = NULL;

    if (c->_state != CALL_STATE_DIALING)
        return -1;

    if (!*c->_peer_number)
        return -1;

    if (account_list_get_size() == 0) {
        notify_no_accounts();
        sflphone_fail (c);
        return -1;
    }

    if (account_list_get_by_state (ACCOUNT_STATE_REGISTERED) == NULL) {
        DEBUG ("Actions: No registered account, cannot make a call");
        notify_no_registered_accounts();
        sflphone_fail (c);
        return -1;
    }

    DEBUG ("Actions: Get account for this call");

    if (strlen(c->_accountID) != 0) {
        DEBUG ("Actions: Account %s already set for this call", c->_accountID);
        current = account_list_get_by_id (c->_accountID);
    } else {
        DEBUG ("Actions: No account set for this call, use first of the list");
        current = account_list_get_current();
    }

    if (current == NULL) {
        DEBUG ("Actions: Unexpected condition: account_t is NULL in %s at %d for accountID %s", __FILE__, __LINE__, c->_accountID);
        return -1;
    }

    if (g_strcasecmp (g_hash_table_lookup (current->properties, "Status"), "REGISTERED") ==0) {
        /* The call is made with the current account */
        // free memory for previous account id and get a new one
        g_free (c->_accountID);
        c->_accountID = g_strdup (current->accountID);
        dbus_place_call (c);
    } else {
        /* Place the call with the first registered account
         * and switch the current account.
         * If we are here, we can be sure that there is at least one.
         */
        current = account_list_get_by_state (ACCOUNT_STATE_REGISTERED);
        g_free (c->_accountID);
        c->_accountID = g_strdup (current->accountID);
        dbus_place_call (c);
        notify_current_account (current);
    }

    c->_history_state = OUTGOING;

    return 0;
}

void
sflphone_place_call (callable_obj_t * c)
{
    DEBUG ("Actions: Placing call with %s @ %s and accountid %s", c->_peer_name, c->_peer_number, c->_accountID);

    if (_is_direct_call (c)) {
        gchar *msg = g_markup_printf_escaped (_ ("Direct SIP call"));
        statusbar_pop_message (__MSG_ACCOUNT_DEFAULT);
        statusbar_push_message (msg , NULL, __MSG_ACCOUNT_DEFAULT);
        g_free (msg);

        place_direct_call (c);
    } else if (place_registered_call (c) < 0)
        DEBUG ("An error occured while placing registered call in %s at %d", __FILE__, __LINE__);
}


void
sflphone_detach_participant (const gchar* callID)
{
    callable_obj_t * selectedCall;
    if (callID == NULL)
        selectedCall = calltab_get_selected_call (current_calls);
    else
        selectedCall = calllist_get_call (current_calls, callID);

    DEBUG ("Action: Detach participant %s", selectedCall->_callID);

    if (selectedCall->_confID) {
        g_free (selectedCall->_confID);
        selectedCall->_confID = NULL;
    }
    // Instant messaging widget should have been deactivated during the conference
    if (selectedCall->_im_widget)
        im_widget_update_state (IM_WIDGET (selectedCall->_im_widget), TRUE);
    calltree_remove_call (current_calls, selectedCall, NULL);
    calltree_add_call (current_calls, selectedCall, NULL);
    dbus_detach_participant (selectedCall->_callID);
}

void
sflphone_join_participant (const gchar* sel_callID, const gchar* drag_callID)
{
    DEBUG ("sflphone join participants %s and %s", sel_callID, drag_callID);

    dbus_join_participant (sel_callID, drag_callID);
}


void
sflphone_add_participant (const gchar* callID, const gchar* confID)
{
    GtkTreeIter iter;
    callable_obj_t *call;

    DEBUG (">SFLphone: Add participant %s to conference %s", callID, confID);

    call = calllist_get_call(current_calls, callID);
    if (call == NULL) {
        ERROR("SFLphone: Error: Could not find call");
        return;
    }

    time(&call->_time_added);

    iter = calltree_get_gtkiter_from_id(history, (gchar *)confID);

    calltree_add_call(history, call, &iter);

    dbus_add_participant (callID, confID);
}

void
sflphone_add_conference()
{
    DEBUG ("sflphone add a conference to tree view");
    // dbus_join_participant(selected_call, dragged_call);
}

void
sflphone_join_conference (const gchar* sel_confID, const gchar* drag_confID)
{
    DEBUG ("sflphone join two conference");
    dbus_join_conference (sel_confID, drag_confID);
}

void
sflphone_add_main_participant (const conference_obj_t * c)
{
    DEBUG ("sflphone add main participant");
    dbus_add_main_participant (c->_confID);
}

void
sflphone_conference_on_hold (const conference_obj_t * c)
{
    DEBUG ("sflphone_conference_on_hold");
    dbus_hold_conference (c);
}

void
sflphone_conference_off_hold (const conference_obj_t * c)
{
    DEBUG ("sflphone_conference_off_hold");
    dbus_unhold_conference (c);
}


void
sflphone_rec_call()
{
    callable_obj_t * selectedCall = calltab_get_selected_call (current_calls);
    conference_obj_t * selectedConf = calltab_get_selected_conf (current_calls);

    if (selectedCall) {
        DEBUG ("SFLphone: Set record for selected call");
        dbus_set_record (selectedCall->_callID);

        switch (selectedCall->_state) {
            case CALL_STATE_CURRENT:
                selectedCall->_state = CALL_STATE_RECORD;
                break;
            case CALL_STATE_RECORD:
                selectedCall->_state = CALL_STATE_CURRENT;
                break;
            default:
                WARN ("Should not happen in sflphone_off_hold ()!");
                break;
        }
        calltree_update_call (current_calls, selectedCall, NULL);
    } else if (selectedConf) {
        DEBUG ("SFLphone: Set record for selected conf");
        dbus_set_record (selectedConf->_confID);
        switch (selectedConf->_state) {
            case CONFERENCE_STATE_ACTIVE_ATACHED:
                selectedConf->_state = CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD;
                break;
            case CONFERENCE_STATE_ACTIVE_ATTACHED_RECORD:
                selectedConf->_state = CONFERENCE_STATE_ACTIVE_ATACHED;
                break;
            case CONFERENCE_STATE_ACTIVE_DETACHED:
                selectedConf->_state = CONFERENCE_STATE_ACTIVE_DETACHED_RECORD;
                break;
            case CONFERENCE_STATE_ACTIVE_DETACHED_RECORD:
              selectedConf->_state = CONFERENCE_STATE_ACTIVE_DETACHED_RECORD;
              break;
            default:
                WARN ("Should not happen in sflphone_off_hold ()!");
                break;
        }
        calltree_update_conference(current_calls, selectedConf);
    }

    update_actions();
}

void sflphone_fill_codec_list ()
{
    guint account_list_size = account_list_get_size();

    for (guint i = 0; i < account_list_size; i++) {
        account_t *current = account_list_get_nth(i);

        if (current)
            sflphone_fill_codec_list_per_account(current);
    }
}

void sflphone_fill_codec_list_per_account (account_t *account)
{
    gchar **order = dbus_get_active_audio_codec_list(account->accountID);

    GQueue *codeclist = account->codecs;

    // First clean the list
    codec_list_clear(&codeclist);

    if (!(*order))
        ERROR ("SFLphone: No codec list provided");
    else {
        for (gchar **pl = order; *pl; pl++) {
            codec_t * cpy = NULL;

            // Each account will have a copy of the system-wide capabilities
            codec_create_new_from_caps (codec_list_get_by_payload ( (gconstpointer) (size_t) atoi (*pl), NULL), &cpy);

            if (cpy) {
                cpy->is_active = TRUE;
                codec_list_add (cpy, &codeclist);
            } else
                ERROR ("SFLphone: Couldn't find codec");
        }
    }

    guint caps_size = codec_list_get_size ();

    for (guint i = 0; i < caps_size; i++) {
        codec_t * current_cap = capabilities_get_nth (i);

        // Check if this codec has already been enabled for this account
        if (codec_list_get_by_payload ( (gconstpointer) (size_t) (current_cap->_payload), codeclist) == NULL) {
            current_cap->is_active = FALSE;
            codec_list_add (current_cap, &codeclist);
        }
    }
    account->codecs = codeclist;
}

void sflphone_fill_call_list (void)
{
    gchar** calls = (gchar**) dbus_get_call_list();
    GHashTable *call_details;

    DEBUG ("sflphone_fill_call_list");

    if (!calls)
        return;

    for (; *calls; calls++) {
        callable_obj_t *c = g_new0 (callable_obj_t, 1);
        gchar *callID = (gchar*) (*calls);
        call_details = dbus_get_call_details (callID);
        c = create_new_call_from_details (callID, call_details);
        g_free(callID);
        c->_zrtp_confirmed = FALSE;
        // Add it to the list
        DEBUG ("Add call retrieved from server side: %s\n", c->_callID);
        calllist_add_call (current_calls, c);
        // Update the GUI
        calltree_add_call (current_calls, c, NULL);
    }
}


void sflphone_fill_conference_list (void)
{
    // TODO Fetch the active conferences at client startup

    gchar** conferences;
    GHashTable *conference_details;

    DEBUG ("SFLphone: Fill conference list");

    conferences = dbus_get_conference_list();

    if (conferences) {
        for (; *conferences; conferences++) {
            conference_obj_t *conf = g_new0 (conference_obj_t, 1);
            const gchar * const conf_id = (gchar*) (*conferences);

            conference_details = (GHashTable*) dbus_get_conference_details (conf_id);

            conf = create_new_conference_from_details (conf_id, conference_details);

            conf->_confID = g_strdup (conf_id);

            conferencelist_add (current_calls, conf);
            calltree_add_conference (current_calls, conf);
        }
    }
}

void sflphone_fill_history (void)
{
    gchar **entries, **entries_orig;
    callable_obj_t *history_call, *call;
    QueueElement *element;
    guint i = 0, n = 0;

    entries = entries_orig = dbus_get_history ();

    while (*entries) {
        gchar *current_entry = *entries;

        // Parsed a conference
        if (g_str_has_prefix(current_entry, "9999")) {
            // create a conference entry
            conference_obj_t *history_conf = create_conference_history_entry_from_serialized(current_entry);

            // verify if this conference has been already created yet
            conference_obj_t *conf = conferencelist_get(history, history_conf->_confID);
            // if this conference hasn't been created yet, add it to the conference list
            if (!conf)
                conferencelist_add(history, history_conf);
            else {
                // if this conference was already created since one of the participant have already
                // been unserialized, update the recordfile value
                conf->_recordfile = g_strdup(history_conf->_recordfile);
            }
        }
        else {
            // do something with key and value
            history_call = create_history_entry_from_serialized_form (current_entry);

            // Add it and update the GUI
            calllist_add_call (history, history_call);

            if (history_call->_confID && g_strcmp0(history_call->_confID, "") != 0) {

                // process conference
                conference_obj_t *conf = conferencelist_get(history, history_call->_confID);
                if (!conf) {
                    // conference does not exist yet, create it
                    conf = create_new_conference(CONFERENCE_STATE_ACTIVE_ATACHED, history_call->_confID);
                    conferencelist_add(history, conf);
                }

                // add this participant to the conference
                conference_add_participant(history_call->_callID, conf);

                // conference start timestamp corespond to
                if (conf->_time_start > history_call->_time_added)
                    conf->_time_start = history_call->_time_added;
            }
        }

        g_free(*entries++);
    }
    g_free(entries_orig);

    // fill the treeview with calls
    n = calllist_get_size(history);
    for(i = 0; i < n; i++) {
        element = calllist_get_nth(history, i);
        if(element->type == HIST_CALL) {
            call = element->elem.call;
            calltree_add_call (history, call, NULL);
        }
    }

    // fill the treeview with conferences
    n = conferencelist_get_size(history);
    for(i = 0; i < n; i++) {
        conference_obj_t *conf = conferencelist_get_nth(history, i);
        if (!conf)
            DEBUG("SFLphone: Error: Could not find conference");
        calltree_add_conference(history, conf);
    }
}

#if ! (GLIB_CHECK_VERSION(2,28,0))
static void
g_slist_free_full (GSList         *list,
        GDestroyNotify  free_func)
{
    g_slist_foreach (list, (GFunc) free_func, NULL);
    g_slist_free (list);
}
#endif


static void hist_free_elt(gpointer list)
{
    g_slist_free_full ((GSList *)list, g_free);
}

void sflphone_save_history (void)
{
    QueueElement *current;
    conference_obj_t *conf;

    GHashTable *result = g_hash_table_new_full (NULL, g_str_equal, g_free, hist_free_elt);

    gint size = calllist_get_size (history);
    for (gint i = 0; i < size; i++) {
        current = calllist_get_nth (history, i);
        if (!current) {
            WARN("SFLphone: Warning: %dth element is null", i);
            break;
        }

        gchar *value;
        if(current->type == HIST_CALL)
            value = serialize_history_call_entry (current->elem.call);
        else if(current->type == HIST_CONFERENCE)
            value = serialize_history_conference_entry(current->elem.conf);
        else {
            ERROR("SFLphone: Error: Unknown type for serialization");
            break;
        }
        gchar *key = g_strdup_printf ("%i", (int) current->elem.call->_time_start);

        g_hash_table_replace (result, (gpointer) key,
                g_slist_append(g_hash_table_lookup(result, key),(gpointer) value));
    }

    size = conferencelist_get_size(history);
    for(gint i = 0; i < size; i++) {
        conf = conferencelist_get_nth(history, i);
        if(!conf) {
            DEBUG("SFLphone: Error: Could not get %dth conference", i);
            break;
        }

        gchar *value = serialize_history_conference_entry(conf);
        gchar *key = g_strdup_printf ("%i", (int) conf->_time_start);

        g_hash_table_replace(result, (gpointer) key,
                g_slist_append(g_hash_table_lookup(result, key), (gpointer) value));
    }

    gchar **ordered_result = sflphone_order_history_hash_table(result);
    dbus_set_history (ordered_result);
    g_strfreev(ordered_result);
    g_hash_table_unref (result);
}

void
sflphone_srtp_sdes_on (callable_obj_t * c)
{
    c->_srtp_state = SRTP_STATE_SDES_SUCCESS;

    calltree_update_call (current_calls, c, NULL);
    update_actions();
}

void
sflphone_srtp_sdes_off (callable_obj_t * c)
{
    c->_srtp_state = SRTP_STATE_UNLOCKED;

    calltree_update_call (current_calls, c, NULL);
    update_actions();
}


void
sflphone_srtp_zrtp_on (callable_obj_t * c)
{
    c->_srtp_state = SRTP_STATE_ZRTP_SAS_UNCONFIRMED;

    calltree_update_call (current_calls, c, NULL);
    update_actions();
}

void
sflphone_srtp_zrtp_off (callable_obj_t * c)
{
    c->_srtp_state = SRTP_STATE_UNLOCKED;
    calltree_update_call (current_calls, c, NULL);
    update_actions();
}

void
sflphone_srtp_zrtp_show_sas (callable_obj_t * c, const gchar* sas, const gboolean verified)
{
    c->_sas = g_strdup (sas);
    c->_srtp_state = verified ? SRTP_STATE_ZRTP_SAS_CONFIRMED : SRTP_STATE_ZRTP_SAS_UNCONFIRMED;

    calltree_update_call (current_calls, c, NULL);
    update_actions();
}

void
sflphone_request_go_clear (void)
{
    callable_obj_t * selectedCall = calltab_get_selected_call (current_calls);

    if (selectedCall)
        dbus_request_go_clear (selectedCall);
}

void
sflphone_call_state_changed (callable_obj_t * c, const gchar * description, const guint code)
{
    DEBUG ("SFLPhone: Call State changed %s", description);

    if (c == NULL) {
        ERROR ("SFLphone: Error: callable obj is NULL in %s at %d", __FILE__, __LINE__);
        return;
    }

    c->_state_code_description = g_strdup (description);
    c->_state_code = code;

    calltree_update_call (current_calls, c, NULL);
    update_actions();
}

