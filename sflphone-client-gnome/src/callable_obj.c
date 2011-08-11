/*
 *  Copyright (C) 2004, 2005, 2006, 2008, 2009, 2010, 2011 Savoir-Faire Linux Inc.
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

#include <callable_obj.h>
#include <codeclist.h>
#include <sflphone_const.h>
#include <time.h>
#include "contacts/calltree.h"
#include  <unistd.h>


#define UNIX_DAY			86400
#define UNIX_WEEK			86400 * 6
#define UNIX_TWO_DAYS		        86400 * 2

gint get_state_callstruct (gconstpointer a, gconstpointer b)
{
    callable_obj_t * c = (callable_obj_t*) a;

    if (c->_state == * ( (call_state_t*) b)) {
        return 0;
    } else {
        return 1;
    }
}

gchar* call_get_peer_name (const gchar *format)
{
    const gchar *end, *name;

    DEBUG ("    callable_obj: %s", format);

    end = g_strrstr (format, "<");

    if (!end) {
        return g_strndup (format, 0);
    } else {
        name = format;
        return g_strndup (name, end - name);
    }
}

gchar* call_get_peer_number (const gchar *format)
{
    DEBUG ("    callable_obj: %s", format);

    gchar * number = g_strrstr (format, "<") + 1;
    gchar * end = g_strrstr (format, ">");

    if (end && number)
        number = g_strndup (number, end - number);
    else
        number = g_strdup (format);

    return number;
}

gchar* call_get_audio_codec (callable_obj_t *obj)
{
    if (obj) {
        const gchar * const audio_codec = dbus_get_current_audio_codec_name (obj);
        const codec_t * const codec = codec_list_get_by_name (audio_codec, NULL);
        if (codec)
            return g_markup_printf_escaped ("%s/%i", audio_codec, codec->sample_rate);
    }

    return g_strdup("");
}

void call_add_error (callable_obj_t * call, gpointer dialog)
{
    g_ptr_array_add (call->_error_dialogs, dialog);
}

void call_remove_error (callable_obj_t * call, gpointer dialog)
{
    g_ptr_array_remove (call->_error_dialogs, dialog);
}

void call_remove_all_errors (callable_obj_t * call)
{
    g_ptr_array_foreach (call->_error_dialogs, (GFunc) gtk_widget_destroy, NULL);
}

void threaded_clock_incrementer (void *pc)
{

    callable_obj_t *call = (callable_obj_t *) pc;


    while (call->clockStarted) {

        int duration;
        time_t start, current;

        gdk_threads_enter ();

        set_timestamp (& (call->_time_current));

        start = call->_time_start;
        current = call->_time_current;

        if (current == start) {
            g_snprintf (call->_timestr, 20, "00:00");

        }

        duration = (int) difftime (current, start);

        if (duration / 60 == 0) {
            if (duration < 10) {
                g_snprintf (call->_timestr, 20, "00:0%d", duration);
            } else {
                g_snprintf (call->_timestr, 20, "00:%d", duration);
            }
        } else {
            if (duration%60 < 10) {
                g_snprintf (call->_timestr, 20, "0%d:0%d", duration/60, duration%60);
            } else {
                g_snprintf (call->_timestr, 20, "%d:%d", duration/60, duration%60);
            }
        }

        // Update clock only if call is active (current, hold, recording transfer)
        if ( (call->_state != CALL_STATE_INVALID) &&
                (call->_state != CALL_STATE_INCOMING) &&
                (call->_state != CALL_STATE_RINGING) &&
                (call->_state != CALL_STATE_DIALING) &&
                (call->_state != CALL_STATE_FAILURE) &&
                (call->_state != CALL_STATE_BUSY)) {
            calltree_update_clock();
        }

        // gdk_flush();
        gdk_threads_leave ();


        usleep (1000000);
    }

    DEBUG ("CallableObj: Stopping Thread");

    g_thread_exit (NULL);

}

void stop_call_clock (callable_obj_t *c)
{

    DEBUG ("CallableObj: Stop call clock");

    if (!c) {
        ERROR ("CallableObj: Callable object is NULL");
        return;
    }

    if (c->_type == CALL && c->clockStarted) {
        c->clockStarted = 0;
        /// no need to join here, only need to call g_thread_exit at the end of the threaded function
        // g_thread_join (c->tid);
    }
}

void create_new_call (callable_type_t type, call_state_t state,
                      const gchar* const callID,
                      const gchar* const accountID,
                      const gchar* const peer_name,
                      const gchar* const peer_number,
                      callable_obj_t ** new_call)
{
    GError *err1 = NULL ;
    callable_obj_t *obj;

    DEBUG ("CallableObj: Create new call");

    DEBUG ("CallableObj: Account: %s", accountID);

    // Allocate memory
    obj = g_new0 (callable_obj_t, 1);

    obj->_error_dialogs = g_ptr_array_new();

    // Set fields
    obj->_type = type;
    obj->_state = state;
    obj->_state_code = 0;
    obj->_state_code_description = NULL;

    if (g_strcasecmp (callID, "") == 0)
    {
        obj->_callID = g_new0 (gchar, 30);
        if (obj->_callID)
            g_sprintf (obj->_callID, "%d", rand());
    }
    else
        obj->_callID = g_strdup (callID);

    obj->_confID = NULL;
    obj->_historyConfID = NULL;
    obj->_accountID = g_strdup (accountID);

    set_timestamp (& (obj->_time_start));
    set_timestamp (& (obj->_time_current));
    set_timestamp (& (obj->_time_stop));

    obj->_srtp_cipher = NULL;
    obj->_sas = NULL;
    obj->_peer_name = g_strdup (peer_name);
    obj->_peer_number = g_strdup (peer_number);
    obj->_trsft_to = NULL;
    obj->_peer_info = get_peer_info (peer_name, peer_number);
    obj->_audio_codec = NULL;
    obj->_recordfile = NULL;
    obj->_record_is_playing = FALSE;

    obj->clockStarted = 1;

    if (obj->_type == CALL) {
        // pthread_create(&(obj->tid), NULL, threaded_clock_incrementer, obj);
        if ( (obj->tid = g_thread_create ( (GThreadFunc) threaded_clock_incrementer, (void *) obj, TRUE, &err1)) == NULL) {
            DEBUG ("Thread creation failed!");
            g_error_free (err1) ;
        }
    }

    obj->_time_added = 0;

    *new_call = obj;
}

void create_new_call_from_details (const gchar *call_id, GHashTable *details, callable_obj_t **call)
{
    callable_obj_t *new_call;
    call_state_t state;

    const gchar * const accountID = g_hash_table_lookup (details, "ACCOUNTID");
    const gchar * const peer_number = g_hash_table_lookup (details, "PEER_NUMBER");
    const gchar * const peer_name = g_hash_table_lookup (details, "DISPLAY_NAME");
    const gchar * const state_str = g_hash_table_lookup (details, "CALL_STATE");

    if (g_strcasecmp (state_str, "CURRENT") == 0)
        state = CALL_STATE_CURRENT;

    else if (g_strcasecmp (state_str, "RINGING") == 0)
        state = CALL_STATE_RINGING;

    else if (g_strcasecmp (state_str, "INCOMING") == 0)
        state = CALL_STATE_INCOMING;

    else if (g_strcasecmp (state_str, "HOLD") == 0)
        state = CALL_STATE_HOLD;

    else if (g_strcasecmp (state_str, "BUSY") == 0)
        state = CALL_STATE_BUSY;

    else
        state = CALL_STATE_FAILURE;

    create_new_call (CALL, state, call_id, accountID, peer_name, call_get_peer_number (peer_number), &new_call);
    *call = new_call;
}

void create_history_entry_from_serialized_form (gchar *entry, callable_obj_t **call)
{
    const gchar *peer_name = "";
    const gchar *peer_number = "";
    const gchar *callID = "";
    const gchar *accountID = "";
    const gchar *time_start = "";
    const gchar *time_stop = "";
    const gchar *recordfile = "";
    const gchar *confID = "";
    const gchar *time_added = "";
    callable_obj_t *new_call;
    history_state_t history_state = MISSED;
    gint token = 0;
    gchar ** ptr;
    gchar ** ptr_orig;
    static const gchar * const delim = "|";
 
    ptr = g_strsplit(entry, delim, 10);
    ptr_orig = ptr;
    while (ptr != NULL && token < 10) {
        switch (token) {
            case 0:
                history_state = get_history_state_from_id (*ptr);
                break;
            case 1:
                peer_number = *ptr;
                break;
            case 2:
                peer_name = *ptr;
                break;
            case 3:
		time_start = *ptr;
		break;
	    case 4:
                time_stop = *ptr;
                break;
	    case 5:
		callID = *ptr;
		break;
            case 6:
                accountID = *ptr;
                break;
            case 7:
		recordfile = *ptr;
		break;
	    case 8:
		confID = *ptr;
		break;
	    case 9:
		time_added = *ptr;
		break;
            default:
                break;
        }

        token++;
        ptr++;
    }

    if (g_strcasecmp (peer_name, "empty") == 0)
        peer_name = "";

    create_new_call (HISTORY_ENTRY, CALL_STATE_DIALING, callID, accountID, peer_name, peer_number, &new_call);
    new_call->_history_state = history_state;
    new_call->_time_start = convert_gchar_to_timestamp (time_start);
    new_call->_time_stop = convert_gchar_to_timestamp (time_stop);
    new_call->_recordfile = g_strdup(recordfile);
    new_call->_confID = g_strdup(confID);
    new_call->_historyConfID = g_strdup(confID);
    new_call->_time_added = convert_gchar_to_timestamp(time_start);
    new_call->_record_is_playing = FALSE;

    *call = new_call;
    g_strfreev(ptr_orig);
}

void free_callable_obj_t (callable_obj_t *c)
{
    DEBUG ("CallableObj: Free callable object");

    stop_call_clock (c);

    g_free (c->_callID);
    g_free (c->_confID);
    g_free (c->_historyConfID);
    g_free (c->_accountID);
    g_free (c->_srtp_cipher);
    g_free (c->_sas);
    g_free (c->_peer_name);
    g_free (c->_peer_number);
    g_free (c->_trsft_to);
    g_free (c->_peer_info);
    g_free (c->_audio_codec);
    g_free (c->_recordfile);

    g_free (c);

    calltree_update_clock();
}

void attach_thumbnail (callable_obj_t *call, GdkPixbuf *pixbuf)
{
    call->_contact_thumbnail = pixbuf;
}

gchar* get_peer_info (const gchar* const number, const gchar* const name)
{
    return g_strconcat ("\"", name, "\" <", number, ">", NULL);
}

history_state_t get_history_state_from_id (gchar *indice)
{

    history_state_t state;

    if (g_strcasecmp (indice, "0") ==0)
        state = MISSED;
    else if (g_strcasecmp (indice, "1") ==0)
        state = INCOMING;
    else if (g_strcasecmp (indice, "2") ==0)
        state = OUTGOING;
    else
        state = MISSED;

    return state;
}

gchar* get_call_duration (callable_obj_t *obj)
{

    gchar *res;
    int duration;
    time_t start, end;

    start = obj->_time_start;
    end = obj->_time_stop;

    if (start == end)
        return g_markup_printf_escaped ("<small>Duration:</small> 0:00");

    duration = (int) difftime (end, start);

    if (duration / 60 == 0) {
        if (duration < 10)
            res = g_markup_printf_escaped ("00:0%i", duration);
        else
            res = g_markup_printf_escaped ("00:%i", duration);
    } else {
        if (duration%60 < 10)
            res = g_markup_printf_escaped ("%i:0%i" , duration/60 , duration%60);
        else
            res = g_markup_printf_escaped ("%i:%i" , duration/60 , duration%60);
    }

    return g_markup_printf_escaped ("<small>Duration:</small> %s", res);

}

static const gchar* get_history_id_from_state (history_state_t state)
{
    static const gchar *tab[LAST] = { "0", "1", "2" };
    if (state >= LAST)
        return "";
    return tab[state];
}

gchar* serialize_history_call_entry (callable_obj_t *entry)
{
    // "0|514-276-5468|Savoir-faire Linux|144562458" for instance
    gchar *peer_number, *peer_name, *account_id;
    static const gchar * const separator = "|";
    gchar *time_start, *time_stop ;
    gchar *record_file;
    gchar *confID , *time_added;

    gchar *call_id = entry->_callID;

    // Need the string form for the history state
    const gchar *history_state = get_history_id_from_state (entry->_history_state);
    // and the timestamps
    time_start = convert_timestamp_to_gchar (entry->_time_start);
    time_stop = convert_timestamp_to_gchar (entry->_time_stop);
    time_added = convert_timestamp_to_gchar (entry->_time_added);

    peer_number = (entry->_peer_number == NULL) ? "" : entry->_peer_number;
    peer_name = (entry->_peer_name == NULL || g_strcasecmp (entry->_peer_name,"") == 0) ? "empty": entry->_peer_name;
    account_id = (entry->_accountID == NULL || g_strcasecmp (entry->_accountID,"") == 0) ? "empty": entry->_accountID;

    confID = (entry->_historyConfID == NULL) ? "" : entry->_historyConfID;
    DEBUG("==================================== SERIALIZE: CONFID %s", confID);

    record_file = (entry->_recordfile == NULL) ? "" : entry->_recordfile;

    gchar *result = g_strconcat (history_state, separator,
                          entry->_peer_number, separator,
                          peer_name, separator,
                          time_start, separator,
			  time_stop, separator,
			  call_id, separator,
                          account_id, separator,
			  record_file, separator,
			  confID, separator,
			  time_added, NULL);
    g_free(time_start);
    g_free(time_stop);
    g_free(time_added);
    return result;
}

// gchar* get_formatted_start_timestamp (callable_obj_t *obj)
gchar *get_formatted_start_timestamp (time_t time_start)
{

    struct tm* ptr;
    time_t lt, now;
    unsigned char str[100];

    // Fetch the current timestamp
    (void) time (&now);
    lt = time_start;

    ptr = localtime (&lt);

    if (now - lt < UNIX_WEEK) {
        if (now-lt < UNIX_DAY) {
            strftime ( (char *) str, 100, N_ ("today at %R"), (const struct tm *) ptr);
        } else {
            if (now - lt < UNIX_TWO_DAYS) {
                strftime ( (char *) str, 100, N_ ("yesterday at %R"), (const struct tm *) ptr);
            } else {
                strftime ( (char *) str, 100, N_ ("%A at %R"), (const struct tm *) ptr);
            }
        }
    } else {
        strftime ( (char *) str, 100, N_ ("%x at %R"), (const struct tm *) ptr);
    }

    // result function of the current locale
    return g_markup_printf_escaped ("\n%s\n" , str);
}

void set_timestamp (time_t *timestamp)
{
    time_t tmp;

    // Set to the current value
    (void) time (&tmp);
    *timestamp=tmp;
}

gchar* convert_timestamp_to_gchar (const time_t timestamp)
{
    return g_markup_printf_escaped ("%i", (int) timestamp);
}

time_t convert_gchar_to_timestamp (const gchar *timestamp)
{
    return (time_t) atoi (timestamp);
}

gchar*
get_peer_information (callable_obj_t *c)
{

    if (g_strcasecmp (c->_peer_name, "") == 0)
        return g_strdup (c->_peer_number);
    else
        return g_strdup (c->_peer_name);
}


