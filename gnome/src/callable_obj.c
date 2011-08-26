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
#include <unistd.h>
#include <assert.h>


gint get_state_callstruct (gconstpointer a, gconstpointer b)
{
    callable_obj_t * c = (callable_obj_t*) a;
    call_state_t state = *((call_state_t*)b);

    return c->_state == state ? 0 : 1;
}

gchar* call_get_peer_name (const gchar *format)
{
    const gchar *end = g_strrstr (format, "<");
    return g_strndup (format, end ? end - format : 0);
}

gchar* call_get_peer_number (const gchar *format)
{
    gchar *number = g_strrstr (format, "<") + 1;
    gchar *end = g_strrstr (format, ">");

    if (end && number)
        return g_strndup (number, end - number);
    else
        return g_strdup (format);
}

gchar* call_get_video_codec (callable_obj_t *obj)
{
    return dbus_get_current_video_codec_name (obj);
}

gchar* call_get_audio_codec (callable_obj_t *obj)
{
    gchar *ret = NULL;
    gchar *audio_codec = NULL;
    if (!obj)
        goto out;

    audio_codec = dbus_get_current_audio_codec_name (obj);
    if (!audio_codec)
        goto out;

    account_t *acc = account_list_get_by_id(obj->_accountID);
    if (!acc)
        goto out;

    const codec_t *const codec = codec_list_get_by_name (audio_codec, acc->codecs);
    if (!codec)
        goto out;

    ret = g_strdup_printf("%s/%i", audio_codec, codec->sample_rate);

out:
    g_free(audio_codec);
    if (ret == NULL)
        return g_strdup("");
    return ret;
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

callable_obj_t *create_new_call (callable_type_t type, call_state_t state,
                      const gchar* const callID,
                      const gchar* const accountID,
                      const gchar* const peer_name,
                      const gchar* const peer_number)
{
    DEBUG ("CallableObj: Create new call (Account: %s)", accountID);

    callable_obj_t *obj = g_new0 (callable_obj_t, 1);

    obj->_error_dialogs = g_ptr_array_new();
    obj->_type = type;
    obj->_state = state;
    obj->_callID = *callID ? g_strdup (callID) : g_strdup_printf("%d", rand());
    obj->_accountID = g_strdup (accountID);

    time (&obj->_time_start);
    time (&obj->_time_stop);

    obj->_peer_name = g_strdup (peer_name);
    obj->_peer_number = g_strdup (peer_number);
    obj->_peer_info = get_peer_info (peer_name, peer_number);

    return obj;
}

callable_obj_t *create_new_call_from_details (const gchar *call_id, GHashTable *details)
{
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

    gchar *number = call_get_peer_number (peer_number);
    callable_obj_t *c = create_new_call (CALL, state, call_id, accountID, peer_name, number);
    g_free(number);
    return c;
}

callable_obj_t *create_history_entry_from_serialized_form (const gchar *entry)
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
    history_state_t history_state = MISSED;

    gchar **ptr_orig = g_strsplit(entry, "|", 10);
    gchar **ptr;
    gint token;
    for (ptr = ptr_orig, token = 0; ptr && token < 10; token++, ptr++)
        switch (token) {
            case 0:     history_state = get_history_state_from_id (*ptr); break;
            case 1:     peer_number = *ptr;     break;
            case 2:     peer_name = *ptr;       break;
            case 3:     time_start = *ptr;      break;
            case 4:     time_stop = *ptr;       break;
            case 5:     callID = *ptr;          break;
            case 6:     accountID = *ptr;       break;
            case 7:     recordfile = *ptr;      break;
            case 8:     confID = *ptr;          break;
            case 9:     time_added = *ptr;      break;
            default:                            break;
        }

    if (g_strcasecmp (peer_name, "empty") == 0)
        peer_name = "";

    callable_obj_t *new_call = create_new_call (HISTORY_ENTRY, CALL_STATE_DIALING, callID, accountID, peer_name, peer_number);
    new_call->_history_state = history_state;
    new_call->_time_start = atoi(time_start);
    new_call->_time_stop = atoi(time_stop);
    new_call->_recordfile = g_strdup(recordfile);
    new_call->_confID = g_strdup(confID);
    new_call->_historyConfID = g_strdup(confID);
    new_call->_time_added = atoi(time_start);
    new_call->_record_is_playing = FALSE;

    g_strfreev(ptr_orig);
    return new_call;
}

void free_callable_obj_t (callable_obj_t *c)
{
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
    g_free (c->_recordfile);

    g_free (c);
}

gchar* get_peer_info (const gchar* const number, const gchar* const name)
{
    return g_strconcat ("\"", name, "\" <", number, ">", NULL);
}

history_state_t get_history_state_from_id (gchar *indice)
{
    history_state_t state = atoi(indice);

    if (state > LAST)
        state = MISSED;

    return state;
}

gchar* get_call_duration (callable_obj_t *obj)
{
    long duration = difftime (obj->_time_stop, obj->_time_start);
    if (duration < 0)
        duration = 0;
    return g_strdup_printf("<small>Duration:</small> %.2ld:%.2ld" , duration/60 , duration%60);
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

    // Need the string form for the history state
    const gchar *history_state = get_history_id_from_state (entry->_history_state);
    // and the timestamps
    time_start = g_strdup_printf ("%i", (int) entry->_time_start);
    time_stop = g_strdup_printf ("%i", (int) entry->_time_stop);
    time_added = g_strdup_printf ("%i", (int) entry->_time_added);

    peer_number = entry->_peer_number ? entry->_peer_number : "";
    peer_name = (entry->_peer_name && *entry->_peer_name) ? entry->_peer_name : "empty";
    account_id = (entry->_accountID && *entry->_accountID) ? entry->_accountID : "empty";

    confID = entry->_historyConfID ? entry->_historyConfID : "";
    record_file = entry->_recordfile ? entry->_recordfile : "";

    gchar *result = g_strconcat (history_state, separator,
                          peer_number, separator,
                          peer_name, separator,
                          time_start, separator,
			  time_stop, separator,
			  entry->_callID, separator,
                          account_id, separator,
			  record_file, separator,
			  confID, separator,
			  time_added, NULL);
    g_free(time_start);
    g_free(time_stop);
    g_free(time_added);
    return result;
}

gchar *get_formatted_start_timestamp (time_t start)
{
    time_t now = time (NULL);
    struct tm start_tm;

    localtime_r (&start, &start_tm);
    time_t diff = now - start;
    if (diff < 0)
        diff = 0;
    const char *fmt;

    if (diff < 60 * 60 * 24 * 7) { // less than 1 week
        if (diff < 60 * 60 * 24) { // less than 1 day
            fmt = N_("today at %R");
        } else {
            if (diff < 60 * 60 * 24 * 2) { // less than 2 days
                fmt = N_("yesterday at %R");
            } else { // between 2 days and 1 week
                fmt = N_("%A at %R");
            }
        }
    } else { // more than 1 week
        fmt = N_("%x at %R");
    }

    char str[100];
    strftime(str, sizeof str, fmt, &start_tm);
    return g_markup_printf_escaped ("%s\n", str);
}
