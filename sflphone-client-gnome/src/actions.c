/*
 *  Copyright (C) 2004, 2005, 2006, 2009, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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
#include <glib/gprintf.h>
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
#include "contacts/searchbar.h"
#include "contacts/addrbookfactory.h"
#include "icons/icon_factory.h"
#include "imwindow.h"
#include "statusicon.h"
#include "widget/imwidget.h"


static GHashTable * ip2ip_profile=NULL;

void
sflphone_notify_voice_mail (const gchar* accountID , guint count)
{
    gchar *id;
    gchar *current_id;
    account_t *current;

    // We want to notify only the current account; ie the first in the list
    id = g_strdup (accountID);
    current_id = account_list_get_current_id ();

    DEBUG ("sflphone_notify_voice_mail begin");

    if (g_strcasecmp (id, current_id) != 0 || account_list_get_size() == 0)
        return;

    // Set the number of voice messages for the current account
    current_account_set_message_number (count);
    current = account_list_get_current ();

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

    if (g_strcasecmp (c->_accountID, EMPTY_ENTRY) == 0) {
        if (!g_str_has_prefix (c->_peer_number, "sip:")) {
            gchar * new_number = g_strconcat ("sip:", c->_peer_number, NULL);
            g_free (c->_peer_number);
            c->_peer_number = new_number;
        }

        return 1;
    }

    if (g_str_has_prefix (c->_peer_number, "sip:")) {
        return 1;
    }

    if (g_str_has_prefix (c->_peer_number, "sips:")) {
        return 1;
    }

    return 0;
}


void
status_bar_display_account ()
{
    gchar* msg;
    account_t* acc;

    statusbar_pop_message (__MSG_ACCOUNT_DEFAULT);

    DEBUG ("status_bar_display_account begin");

    acc = account_list_get_current ();

    if (acc) {
        status_tray_icon_online (TRUE);
        msg = g_markup_printf_escaped ("%s %s (%s)" ,
                                       _ ("Using account"),
                                       (gchar*) g_hash_table_lookup (acc->properties , ACCOUNT_ALIAS),
                                       (gchar*) g_hash_table_lookup (acc->properties , ACCOUNT_TYPE));
    } else {
        status_tray_icon_online (FALSE);
        msg = g_markup_printf_escaped (_ ("No registered accounts"));
    }

    statusbar_push_message (msg, NULL,  __MSG_ACCOUNT_DEFAULT);
    g_free (msg);

    DEBUG ("status_bar_display_account_end");
}


gboolean
sflphone_quit ()
{
    gboolean quit = FALSE;
    guint count = calllist_get_size (current_calls);

    if (count > 0) {
        quit = main_window_ask_quit();
    } else {
        quit = TRUE;
    }

    if (quit) {
        // Save the history
        sflphone_save_history ();

        dbus_unregister (getpid());
        dbus_clean ();
        //call_list_clean(); TODO
        //account_list_clean()
        gtk_main_quit ();
    }

    return quit;
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

    // test wether the widget contain text, if not remove it
    if ( (im_window_get_nb_tabs() > 1) && c->_im_widget && ! (IM_WIDGET (c->_im_widget)->containText))
        im_window_remove_tab (c->_im_widget);
    else
        im_widget_update_state (IM_WIDGET (c->_im_widget), FALSE);

#if GTK_CHECK_VERSION(2,10,0)
    status_tray_icon_blink (FALSE);
#endif
    calltree_update_clock();
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
        int number_of_credential = dbus_get_number_of_credential (a->accountID);

        if (number_of_credential) {
            a->credential_information = g_ptr_array_new();
        } else {
            a->credential_information = NULL;
        }

        int credential_index;

        for (credential_index = 0; credential_index < number_of_credential; credential_index++) {
            GHashTable * credential_information = dbus_get_credential (a->accountID, credential_index);
            g_ptr_array_add (a->credential_information, credential_information);
        }

        gchar * status = g_hash_table_lookup (details, REGISTRATION_STATUS);

        if (strcmp (status, "REGISTERED") == 0) {
            a->state = ACCOUNT_STATE_REGISTERED;
        } else if (strcmp (status, "UNREGISTERED") == 0) {
            a->state = ACCOUNT_STATE_UNREGISTERED;
        } else if (strcmp (status, "TRYING") == 0) {
            a->state = ACCOUNT_STATE_TRYING;
        } else if (strcmp (status, "ERROR") == 0) {
            a->state = ACCOUNT_STATE_ERROR;
        } else if (strcmp (status , "ERROR_AUTH") == 0) {
            a->state = ACCOUNT_STATE_ERROR_AUTH;
        } else if (strcmp (status , "ERROR_NETWORK") == 0) {
            a->state = ACCOUNT_STATE_ERROR_NETWORK;
        } else if (strcmp (status , "ERROR_HOST") == 0) {
            a->state = ACCOUNT_STATE_ERROR_HOST;
        } else if (strcmp (status , "ERROR_CONF_STUN") == 0) {
            a->state = ACCOUNT_STATE_ERROR_CONF_STUN;
        } else if (strcmp (status , "ERROR_EXIST_STUN") == 0) {
            a->state = ACCOUNT_STATE_ERROR_EXIST_STUN;
        } else if (strcmp (status, "READY") == 0) {
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
    if (!dbus_connect (error)) {
        return FALSE;
    }

    if (!dbus_register (getpid (), "Gtk+ Client", error)) {
        return FALSE;
    }
    
    abookfactory_init_factory(); 

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

void sflphone_get_ip2ip_properties (GHashTable **properties)
{
    *properties	= ip2ip_profile;
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
                set_timestamp (&selectedCall->_time_stop);

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
                stop_notification();
                break;
            case CALL_STATE_TRANSFERT:
                dbus_hang_up (selectedCall);
                call_remove_all_errors (selectedCall);
                set_timestamp (&selectedCall->_time_stop);
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

    stop_call_clock (selectedCall);

    calltree_update_clock();
}


void
sflphone_conference_hang_up()
{
    conference_obj_t * selectedConf = calltab_get_selected_conf(current_calls);

    if (selectedConf) {
        dbus_hang_up_conference (selectedConf);
    }
}


void
sflphone_pick_up()
{
    callable_obj_t * selectedCall = NULL;
    selectedCall = calltab_get_selected_call (active_calltree);

    DEBUG("SFLphone: Pick up");

    if (selectedCall) {
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
                if (im_window_is_visible())
                    im_widget_display ( (IMWidget **) (&selectedCall->_im_widget), NULL, selectedCall->_callID, NULL);

                dbus_accept (selectedCall);
                stop_notification();
                break;
            case CALL_STATE_HOLD:
                sflphone_new_call();
                break;
            case CALL_STATE_TRANSFERT:
                dbus_transfert (selectedCall);
                set_timestamp (&selectedCall->_time_stop);
                calltree_remove_call(current_calls, selectedCall, NULL);
                calllist_remove_call(current_calls, selectedCall->_callID);
		break;
            case CALL_STATE_CURRENT:
            case CALL_STATE_RECORD:
                sflphone_new_call();
                break;
            case CALL_STATE_RINGING:
                sflphone_new_call();
                break;
            default:
                WARN ("Should not happen in sflphone_pick_up()!");
                break;
        }
    } else {
        sflphone_new_call();
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
        set_timestamp (&c->_time_start);

    c->_state = CALL_STATE_CURRENT;
    calltree_update_call (current_calls, c, NULL);
    update_actions();
}

void
sflphone_record (callable_obj_t * c)
{
    if (c->_state != CALL_STATE_HOLD)
        set_timestamp (&c->_time_start);

    c->_state = CALL_STATE_RECORD;
    calltree_update_call (current_calls, c, NULL);
    update_actions();
}

void
sflphone_set_transfert()
{
    callable_obj_t * c = calltab_get_selected_call (current_calls);

    if (c) {
        c->_state = CALL_STATE_TRANSFERT;
        c->_trsft_to = g_strdup ("");
        calltree_update_call (current_calls, c, NULL);
    }

    update_actions();
}

void
sflphone_unset_transfert()
{
    callable_obj_t * c = calltab_get_selected_call (current_calls);

    if (c) {
        c->_state = CALL_STATE_CURRENT;
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
    gchar *msg = "";

    c->_history_state = MISSED;
    calllist_add_call (current_calls, c);
    calllist_add_call (history, c);
    calltree_add_call (current_calls, c, NULL);
    calltree_add_call (history, c, NULL);
    update_actions();
    calltree_display (current_calls);

    // Change the status bar if we are dealing with a direct SIP call
    if (_is_direct_call (c)) {
        msg = g_markup_printf_escaped (_ ("Direct SIP call"));
        statusbar_pop_message (__MSG_ACCOUNT_DEFAULT);
        statusbar_push_message (msg , NULL, __MSG_ACCOUNT_DEFAULT);
        g_free (msg);
    }
}

void
process_dialing (callable_obj_t *c, guint keyval, gchar *key)
{
    // We stop the tone
    if (strlen (c->_peer_number) == 0 && c->_state != CALL_STATE_TRANSFERT) {
        dbus_start_tone (FALSE , 0);
        //dbus_play_dtmf( key );
    }

    switch (keyval) {
        case 65293: /* ENTER */
        case 65421: /* ENTER numpad */
            sflphone_place_call (c);
            break;
        case 65307: /* ESCAPE */
            sflphone_hang_up (c);
            break;
        case 65288: { /* BACKSPACE */
            /* Brackets mandatory because of local vars */
            gchar * before = c->_peer_number;

            if (strlen (c->_peer_number) >= 1) {

                if (c->_state == CALL_STATE_TRANSFERT) {
                    // Process backspace if and only if string not NULL
                    if (strlen (c->_trsft_to) > 0)
                        c->_trsft_to = g_strndup (c->_trsft_to, strlen (c->_trsft_to) - 1);
                } else {
                    c->_peer_number = g_strndup (c->_peer_number, strlen (c->_peer_number) -1);
                    g_free (before);
                    DEBUG ("SFLphone: TO: backspace %s", c->_peer_number);
                }

                calltree_update_call (current_calls, c, NULL);
            } else if (strlen (c->_peer_number) == 0) {
                if (c->_state != CALL_STATE_TRANSFERT)
                    dbus_hang_up (c);
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
            if (keyval < 127 || (keyval > 65400 && keyval < 65466)) {

                if (c->_state == CALL_STATE_TRANSFERT) {
                    c->_trsft_to = g_strconcat (c->_trsft_to, key, NULL);
                } else {
                    dbus_play_dtmf (key);
                    c->_peer_number = g_strconcat (c->_peer_number, key, NULL);
                }

                if (c->_state == CALL_STATE_DIALING) {
                    //g_free(c->_peer_name);
                    //c->_peer_name = g_strconcat("\"\" <", c->_peer_number, ">", NULL);
                }

                calltree_update_call (current_calls, c, NULL);
            }

            break;
    }
}


callable_obj_t *
sflphone_new_call()
{

    callable_obj_t *c;
    callable_obj_t * current_selected_call;
    gchar *peer_name, *peer_number;

    DEBUG ("Actions: Sflphone new call");

    current_selected_call = calltab_get_selected_call (current_calls);

    if ( (current_selected_call != NULL) && (current_selected_call->_confID == NULL))
        sflphone_on_hold();

    // Play a tone when creating a new call
    if (calllist_get_size (current_calls) == 0)
        dbus_start_tone (TRUE , (current_account_has_new_message ()  > 0) ? TONE_WITH_MESSAGE : TONE_WITHOUT_MESSAGE) ;

    peer_number = g_strdup ("");
    peer_name = g_strdup ("");
    create_new_call (CALL, CALL_STATE_DIALING, "", "", peer_name, peer_number, &c);

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

    DEBUG("SFLphone: Keypad");

    if ( (active_calltree != current_calls) || (active_calltree == current_calls && !c)) {
        DEBUG ("Not in a call, not dialing, create a new call");

        //dbus_play_dtmf(key);
        switch (keyval) {
            case 65293: /* ENTER */
            case 65421: /* ENTER numpad */
            case 65307: /* ESCAPE */
                break;
            default:
                calltree_display (current_calls);
                process_dialing (sflphone_new_call(), keyval, key);
                break;
        }
    } else if (c) {
        DEBUG ("Call is non-zero");

        switch (c->_state) {
            case CALL_STATE_DIALING: // Currently dialing => edit number
                DEBUG ("Writing a number");
                process_dialing (c, keyval, key);
                break;
            case CALL_STATE_RECORD:
            case CALL_STATE_CURRENT:

                switch (keyval) {
                    case 65307: /* ESCAPE */
                        dbus_hang_up (c);
                        set_timestamp (&c->_time_stop);
                        calltree_update_call (history, c, NULL);
                        break;
                    default:
                        // To play the dtmf when calling mail box for instance
                        dbus_play_dtmf (key);

                        if (keyval < 255 || (keyval >65453 && keyval < 65466)) {
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

                switch (keyval) {
                    case 65293: /* ENTER */
                    case 65421: /* ENTER numpad */
                        c->_history_state = INCOMING;
                        calltree_update_call (history, c, NULL);
                        dbus_accept (c);
                        DEBUG ("from sflphone_keypad ( enter ) : ");
                        stop_notification();
                        break;
                    case 65307: /* ESCAPE */
                        dbus_refuse (c);
                        DEBUG ("from sflphone_keypad ( escape ) : ");
                        stop_notification();
                        break;
                }

                break;
            case CALL_STATE_TRANSFERT:

                switch (keyval) {
                    case 65293: /* ENTER */
                    case 65421: /* ENTER numpad */
                        dbus_transfert (c);
                        set_timestamp (&c->_time_stop);
                        calltree_remove_call(current_calls, c, NULL);
			calllist_remove_call(current_calls, c->_callID);
                        break;
                    case 65307: /* ESCAPE */
                        sflphone_unset_transfert (c);
                        break;
                    default: // When a call is on transfert, typing new numbers will add it to c->_peer_number
                        process_dialing (c, keyval, key);
                        break;
                }

                break;
            case CALL_STATE_HOLD:

                switch (keyval) {
                    case 65293: /* ENTER */
                    case 65421: /* ENTER numpad */
                        dbus_unhold (c);
                        break;
                    case 65307: /* ESCAPE */
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

                //c->_stop = 0;
                switch (keyval) {
                    case 65307: /* ESCAPE */
                        dbus_hang_up (c);
                        //c->_stop = 0;
                        calltree_update_call (history, c, NULL);
                        break;
                }

                break;
            default:
                break;
        }

    } else {
        sflphone_new_call();
    }
}

static int _place_direct_call (const callable_obj_t * c)
{
    if (c->_state == CALL_STATE_DIALING) {
        dbus_place_call (c);
    } else {
        return -1;
    }

    return 0;
}

static int _place_registered_call (callable_obj_t * c)
{

    account_t * current = NULL;

    if (c == NULL) {
        DEBUG ("Actions: Callable_obj_t is NULL in _place_registered_call");
        return -1;
    }

    if (c->_state != CALL_STATE_DIALING) {
        return -1;
    }

    if (g_strcasecmp (c->_peer_number, "") == 0) {
        return -1;
    }

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

    if (g_strcasecmp (c->_accountID, "") != 0) {
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
    
    calllist_add_call (history, c);
    calltree_add_call (history, c, NULL);    

    return 0;
}

void
sflphone_place_call (callable_obj_t * c)
{
    gchar *msg = "";

    if (c == NULL) {
        DEBUG ("Actions: Unexpected condition: callable_obj_t is null in %s at %d", __FILE__, __LINE__);
        return;
    }

    DEBUG ("Actions: Placing call with %s @ %s and accountid %s", c->_peer_name, c->_peer_number, c->_accountID);

    if (_is_direct_call (c)) {
        msg = g_markup_printf_escaped (_ ("Direct SIP call"));
        statusbar_pop_message (__MSG_ACCOUNT_DEFAULT);
        statusbar_push_message (msg , NULL, __MSG_ACCOUNT_DEFAULT);
        g_free (msg);

        if (_place_direct_call (c) < 0) {
            DEBUG ("An error occured while placing direct call in %s at %d", __FILE__, __LINE__);
            return;
        }
    } else {
        if (_place_registered_call (c) < 0) {
            DEBUG ("An error occured while placing registered call in %s at %d", __FILE__, __LINE__);
            return;
        }
    }
}


void
sflphone_detach_participant (const gchar* callID)
{
    DEBUG ("Action: Detach participant from conference");

    if (callID == NULL) {
        callable_obj_t * selectedCall = calltab_get_selected_call (current_calls);
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
    } else {
        callable_obj_t * selectedCall = calllist_get_call (current_calls, callID);
        DEBUG ("Action: Darticipant %s", callID);

        if (selectedCall->_confID) {
            g_free (selectedCall->_confID);
            selectedCall->_confID = NULL;
        }

        // Instant messagin widget should have been deactivated during the conference
        if (selectedCall->_im_widget)
            im_widget_update_state (IM_WIDGET (selectedCall->_im_widget), TRUE);

        calltree_remove_call (current_calls, selectedCall, NULL);
        calltree_add_call (current_calls, selectedCall, NULL);
        dbus_detach_participant (callID);
    }

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
    DEBUG ("sflphone add participant %s to conference %s", callID, confID);

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

    guint account_list_size;
    guint i;
    account_t *current = NULL;

    DEBUG ("SFLphone: Fill codec list");

    account_list_size = account_list_get_size ();

    for (i=0; i<account_list_size; i++) {
        current = account_list_get_nth (i);

        if (current) {
            sflphone_fill_codec_list_per_account (&current);
        }
    }

}

void sflphone_fill_codec_list_per_account (account_t **account)
{

    gchar **order;
    gchar** pl;
    GQueue *codeclist;
    gboolean active = FALSE;

    order = (gchar**) dbus_get_active_audio_codec_list ( (*account)->accountID);

    codeclist = (*account)->codecs;

    // First clean the list
    codec_list_clear (&codeclist);

    if (! (*order))
        ERROR ("SFLphone: No codec list provided");

    for (pl=order; *pl; pl++) {
        codec_t * cpy = NULL;

        // Each account will have a copy of the system-wide capabilities
        codec_create_new_from_caps (codec_list_get_by_payload ( (gconstpointer) (size_t) atoi (*pl), NULL), &cpy);

        if (cpy) {
            cpy->is_active = TRUE;
            codec_list_add (cpy, &codeclist);
        } else
            ERROR ("SFLphone: Couldn't find codec");
    }

    // Test here if we just added some active codec.
    active = (codeclist->length == 0) ? TRUE : FALSE;

    guint caps_size = codec_list_get_size (), i=0;

    for (i=0; i<caps_size; i++) {

        codec_t * current_cap = capabilities_get_nth (i);

        // Check if this codec has already been enabled for this account
        if (codec_list_get_by_payload ( (gconstpointer) (size_t) (current_cap->_payload), codeclist) == NULL) {
            // codec_t *cpy;
            // codec_create_new_from_caps (current_cap, &cpy);
            current_cap->is_active = active;
            codec_list_add (current_cap, &codeclist);
        } else {
        }

    }

    (*account)->codecs = codeclist;
}

void sflphone_fill_call_list (void)
{

    gchar** calls = (gchar**) dbus_get_call_list();
    GHashTable *call_details;
    callable_obj_t *c;
    gchar *callID;

    DEBUG ("sflphone_fill_call_list");

    if (calls) {
        for (; *calls; calls++) {
            c = g_new0 (callable_obj_t, 1);
            callID = (gchar*) (*calls);
            call_details = dbus_get_call_details (callID);
            create_new_call_from_details (callID, call_details, &c);
            c->_callID = g_strdup (callID);
            c->_zrtp_confirmed = FALSE;
            // Add it to the list
            DEBUG ("Add call retrieved from server side: %s\n", c->_callID);
            calllist_add_call (current_calls, c);
            // Update the GUI
            calltree_add_call (current_calls, c, NULL);
        }
    }
}


void sflphone_fill_conference_list (void)
{
    // TODO Fetch the active conferences at client startup

    gchar** conferences;
    GHashTable *conference_details;
    gchar* conf_id;
    conference_obj_t* conf;

    DEBUG ("sflphone_fill_conference_list");

    conferences = dbus_get_conference_list();

    if (conferences) {
        for (; *conferences; conferences++) {
            conf = g_new0 (conference_obj_t, 1);
            conf_id = (gchar*) (*conferences);

            DEBUG ("   fetching conference: %s", conf_id);

            conference_details = (GHashTable*) dbus_get_conference_details (conf_id);

            create_new_conference_from_details (conf_id, conference_details, &conf);

            conf->_confID = g_strdup (conf_id);

            conferencelist_add (current_calls, conf);
            calltree_add_conference (current_calls, conf);
        }
    }
}

void sflphone_fill_history (void)
{
    gchar **entries, *current_entry;
    callable_obj_t *history_entry;
    conference_obj_t *conference_entry;
    gchar *value;

    DEBUG ("======================================================= SFLphone: Loading history");

    entries = dbus_get_history ();

    while (*entries) {
    	const gchar *delim = "|";
	gchar **ptr;

        current_entry = *entries;

	ptr = g_strsplit(current_entry, delim, 6);
	if(ptr != NULL) {

	    // first ptr refers to entry type
	    if(g_strcmp0(*ptr, "2188") == 0) {
		DEBUG("------------------------------- SFLphone: Serialized item: %s", *ptr);
		create_conference_history_entry_from_serialized(ptr, &conference_entry);
		conferencelist_add (history, conference_entry);
		conferencelist_add(current_calls, conference_entry);
		calltree_add_conference (history, conference_entry);
	    }
	    else {
                // do something with key and value
                create_history_entry_from_serialized_form (ptr, &history_entry);
                
		// Add it and update the GUI
                calllist_add_call (history, history_entry);
		calltree_add_call (history, history_entry, NULL);
	    }
	}
	entries++;
    }

    DEBUG ("======================================================== SFLphone: Loading history ...(end)");
}

void sflphone_save_history (void)
{
    gint size;
    gint i;
    QueueElement *current;
    conference_obj_t *conf;
    GHashTable *result = NULL;
    gchar *key, *value;

    DEBUG ("==================================================== SFLphone: Saving history");

    result = g_hash_table_new (NULL, g_str_equal);
    size = calllist_get_size (history);

    for (i = 0; i < size; i++) {
        current = calllist_get_nth (history, i);

        if (current) {
	    if(current->type == HIST_CALL) {
                value = serialize_history_call_entry (current->elem.call);
		key =  convert_timestamp_to_gchar (current->elem.call->_time_start);
		DEBUG("--------------------------------- SFLphone: Serialize call [%s]: %s", key, value);
            }
	    else if(current->type == HIST_CONFERENCE) {
                value = serialize_history_conference_entry(current->elem.conf);
		key = convert_timestamp_to_gchar (current->elem.conf->_time_start);
		DEBUG("--------------------------------- SFLphone: Serialize conference [%s]: %s", key, value);
            }
 	    else {
		ERROR("SFLphone: Error: Unknown type for serialization");
            }

            g_hash_table_replace (result, (gpointer) key, (gpointer) value);
        } 
	else {
	    WARN("SFLphone: Warning: %dth element is null", i);
        }
    }

    size = conferencelist_get_size(history);
    DEBUG("SFLphone: Conference list size %d", size);

    while(size > 0) {
	conf = conferencelist_pop_head(history);
	size = conferencelist_get_size(history);

        if(conf) {
	    value = serialize_history_conference_entry(conf);
	    key = convert_timestamp_to_gchar(conf->_time_start);
	    DEBUG("-------------------------------------- SFLphone: Serialize conference [%s]: %s", key, value);
        }
	else {
	    WARN("SFLphone: Warning: %dth element is NULL", i);
        }
	g_hash_table_replace(result, (gpointer)key, (gpointer)value);	
    }

    dbus_set_history (result);

    // Decrement the reference count
    g_hash_table_unref (result);

    
    DEBUG ("==================================================== SFLphone: Saving history (end)");
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
    if (c == NULL) {
        DEBUG ("Panic callable obj is NULL in %s at %d", __FILE__, __LINE__);
    }

    c->_sas = g_strdup (sas);

    if (verified == TRUE) {
        c->_srtp_state = SRTP_STATE_ZRTP_SAS_CONFIRMED;
    } else {
        c->_srtp_state = SRTP_STATE_ZRTP_SAS_UNCONFIRMED;
    }

    calltree_update_call (current_calls, c, NULL);
    update_actions();
}

void
sflphone_srtp_zrtp_not_supported (callable_obj_t * c)
{
    DEBUG ("ZRTP not supported");
    main_window_zrtp_not_supported (c);
}

/* Method on sflphoned */
void
sflphone_set_confirm_go_clear (callable_obj_t * c)
{
    dbus_set_confirm_go_clear (c);
}

void
sflphone_request_go_clear (void)
{
    callable_obj_t * selectedCall = calltab_get_selected_call (current_calls);

    if (selectedCall) {
        dbus_request_go_clear (selectedCall);
    }
}

/* Signal sent by sflphoned */
void
sflphone_confirm_go_clear (callable_obj_t * c)
{
    main_window_confirm_go_clear (c);
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


void sflphone_get_interface_addr_from_name (char *iface_name, char **iface_addr, int size)
{

    struct ifreq ifr;
    int fd;
    int err;
    // static char iface_addr[18];
    char *tmp_addr;

    struct sockaddr_in *saddr_in;
    struct in_addr *addr_in;

    if ( (fd = socket (AF_INET, SOCK_DGRAM,0)) < 0)
        DEBUG ("getInterfaceAddrFromName error could not open socket\n");

    memset (&ifr, 0, sizeof (struct ifreq));

    strcpy (ifr.ifr_name, iface_name);
    ifr.ifr_addr.sa_family = AF_INET;

    if ( (err = ioctl (fd, SIOCGIFADDR, &ifr)) < 0)
        DEBUG ("getInterfaceAddrFromName use default interface (0.0.0.0)\n");


    saddr_in = (struct sockaddr_in *) &ifr.ifr_addr;
    addr_in = & (saddr_in->sin_addr);

    tmp_addr = (char *) addr_in;

    snprintf (*iface_addr, size, "%d.%d.%d.%d",
              UC (tmp_addr[0]), UC (tmp_addr[1]), UC (tmp_addr[2]), UC (tmp_addr[3]));

    close (fd);

}
