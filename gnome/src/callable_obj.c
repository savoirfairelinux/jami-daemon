/*
 *  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
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

#include "callable_obj.h"
#include "str_utils.h"
#include "sflphone_const.h"
#include <time.h>
#include <glib/gi18n.h>
#include "contacts/calltab.h"
#include "contacts/calltree.h"
#include "dbus.h"
#include <unistd.h>
#include <stdint.h>

gint get_state_callstruct(gconstpointer a, gconstpointer b)
{
    callable_obj_t * c = (callable_obj_t*) a;
    call_state_t state = *((call_state_t*)b);

    return c->_state == state ? 0 : 1;
}

gchar* call_get_display_name(const gchar *format)
{
    const gchar *end = g_strrstr(format, "<");
    return g_strndup(format, end ? end - format : 0);
}

gchar* call_get_peer_number(const gchar *format)
{
    gchar *number = g_strrstr(format, "<") + 1;
    gchar *end = g_strrstr(format, ">");

    if (end && number)
        return g_strndup(number, end - number);
    else
        return g_strdup(format);
}

void call_add_error(callable_obj_t * call, gpointer dialog)
{
    g_ptr_array_add(call->_error_dialogs, dialog);
}

void call_remove_error(callable_obj_t * call, gpointer dialog)
{
    g_ptr_array_remove(call->_error_dialogs, dialog);
}

void call_remove_all_errors(callable_obj_t * call)
{
    g_ptr_array_foreach(call->_error_dialogs, (GFunc) gtk_widget_destroy, NULL);
}

callable_obj_t *create_new_call(callable_type_t type, call_state_t state,
                                const gchar* const callID,
                                const gchar* const accountID,
                                const gchar* const display_name,
                                const gchar* const peer_number)
{
    callable_obj_t *obj = g_new0(callable_obj_t, 1);

    obj->_error_dialogs = g_ptr_array_new();
    obj->_type = type;
    obj->_state = state;
    obj->_callID = *callID ? g_strdup(callID) : g_strdup_printf("%d", rand());
    obj->_accountID = g_strdup(accountID);

    time(&obj->_time_start);
    time(&obj->_time_stop);

    obj->_display_name = g_strdup(display_name);
    obj->_peer_number = g_strdup(peer_number);
    obj->_peer_info = get_peer_info(display_name, peer_number);

    return obj;
}

callable_obj_t *create_new_call_from_details(const gchar *call_id, GHashTable *details)
{
    call_state_t state;

    const gchar * const accountID = g_hash_table_lookup(details, "ACCOUNTID");
    const gchar * const peer_number = g_hash_table_lookup(details, "PEER_NUMBER");
    const gchar * const display_name = g_hash_table_lookup(details, "DISPLAY_NAME");
    const gchar * const state_str = g_hash_table_lookup(details, "CALL_STATE");

    if (utf8_case_equal(state_str, "CURRENT"))
        state = CALL_STATE_CURRENT;
    else if (utf8_case_equal(state_str, "RINGING"))
        state = CALL_STATE_RINGING;
    else if (utf8_case_equal(state_str, "INCOMING"))
        state = CALL_STATE_INCOMING;
    else if (utf8_case_equal(state_str, "HOLD"))
        state = CALL_STATE_HOLD;
    else if (utf8_case_equal(state_str, "BUSY"))
        state = CALL_STATE_BUSY;
    else
        state = CALL_STATE_FAILURE;

    gchar *number = call_get_peer_number(peer_number);
    callable_obj_t *c = create_new_call(CALL, state, call_id, accountID, display_name, number);
    g_free(number);
    return c;
}

static gconstpointer get_str(GHashTable *entry, gconstpointer key)
{
    gconstpointer result = g_hash_table_lookup(entry, key);
    if (!result || g_strcmp0(result, "empty") == 0)
        result = "";
    return result;
}

/* FIXME:tmatth: These need to be in sync with the daemon */
static const char * const ACCOUNT_ID_KEY =        "accountid";
static const char * const CALLID_KEY =            "callid";
static const char * const CONFID_KEY =            "confid";
static const char * const DISPLAY_NAME_KEY =      "display_name";
static const char * const PEER_NUMBER_KEY =       "peer_number";
static const char * const RECORDING_PATH_KEY =    "recordfile";
static const char * const STATE_KEY =             "state";
static const char * const TIMESTAMP_STOP_KEY =    "timestamp_stop";

callable_obj_t *create_history_entry_from_hashtable(GHashTable *entry)
{
    gconstpointer callID = get_str(entry, CALLID_KEY);
    gconstpointer accountID =  get_str(entry, ACCOUNT_ID_KEY);
    gconstpointer display_name =  get_str(entry, DISPLAY_NAME_KEY);
    gconstpointer peer_number =  get_str(entry, PEER_NUMBER_KEY);
    callable_obj_t *new_call = create_new_call(HISTORY_ENTRY, CALL_STATE_DIALING, callID, accountID, display_name, peer_number);
    new_call->_history_state = g_strdup(get_str(entry, STATE_KEY));
    gconstpointer value =  g_hash_table_lookup(entry, TIMESTAMP_START_KEY);
    new_call->_time_start = value ? atoi(value) : 0;
    value =  g_hash_table_lookup(entry, TIMESTAMP_STOP_KEY);
    new_call->_time_stop = value ? atoi(value) : 0;
    new_call->_recordfile = g_strdup(g_hash_table_lookup(entry, RECORDING_PATH_KEY));
    new_call->_historyConfID = g_strdup(g_hash_table_lookup(entry, CONFID_KEY));
    new_call->_record_is_playing = FALSE;

    return new_call;
}

void free_callable_obj_t (callable_obj_t *c)
{
    g_free(c->_callID);
    g_free(c->_historyConfID);
    g_free(c->_accountID);
    g_free(c->_sas);
    g_free(c->_display_name);
    g_free(c->_peer_number);
    g_free(c->_trsft_to);
    g_free(c->_peer_info);
    g_free(c->_recordfile);

    g_free(c);
}

gchar* get_peer_info(const gchar* const number, const gchar* const name)
{
    return g_strconcat("\"", name, "\" <", number, ">", NULL);
}

gchar* get_call_duration(callable_obj_t *obj)
{
    char time_str[32];
    format_duration(obj, obj->_time_stop, time_str, sizeof time_str);

    return g_strdup_printf("<small>Duration:</small> %s", time_str);
}

void
format_duration(callable_obj_t *obj, time_t end, char *timestr, size_t timestr_sz)
{
    const gdouble diff = difftime(end, obj->_time_start);
    guint32 seconds = CLAMP(diff, 0.0f, UINT32_MAX);

    enum {HOURS_PER_DAY = 24, DAYS_PER_YEAR = 365, SECONDS_PER_HOUR = 3600,
          SECONDS_PER_DAY = SECONDS_PER_HOUR * HOURS_PER_DAY,
          SECONDS_PER_YEAR = DAYS_PER_YEAR * SECONDS_PER_DAY};

    const guint32 years =  seconds / SECONDS_PER_YEAR;
    const guint32 days =  (seconds / SECONDS_PER_DAY) % DAYS_PER_YEAR;
    const guint32 hours = (seconds / SECONDS_PER_HOUR) % HOURS_PER_DAY;
    const guint32 minutes = (seconds / 60) % 60;
    seconds %= 60;

    if (years)
        g_snprintf(timestr, timestr_sz, _("%uy %ud %02uh %02umn %02us"),
                years, days, hours, minutes, seconds);
    else if (days)
        g_snprintf(timestr, timestr_sz, _("%ud %02uh %02umn %02us"),
                days, hours, minutes, seconds);
    else if (hours)
        g_snprintf(timestr, timestr_sz, "%u:%02u:%02u",
                hours, minutes, seconds);
    else
        g_snprintf(timestr, timestr_sz, "%02u:%02u",
                minutes, seconds);
}

static
void add_to_hashtable(GHashTable *hashtable, const gchar *key, const gchar *value)
{
    g_hash_table_insert(hashtable, g_strdup(key), g_strdup(value));
}

GHashTable* create_hashtable_from_history_entry(callable_obj_t *entry)
{
    const gchar *history_state = entry->_history_state ? entry->_history_state : "";
    // and the timestamps
    gchar *time_start = g_strdup_printf("%i", (int) entry->_time_start);
    gchar *time_stop = g_strdup_printf("%i", (int) entry->_time_stop);

    const gchar *call_id = entry->_callID ? entry->_callID : "";
    const gchar *peer_number = entry->_peer_number ? entry->_peer_number : "";
    const gchar *display_name = (entry->_display_name && *entry->_display_name) ? entry->_display_name : "empty";
    const gchar *account_id = (entry->_accountID && *entry->_accountID) ? entry->_accountID : "empty";

    const gchar *conf_id = entry->_historyConfID ? entry->_historyConfID : "";
    const gchar *recording_path = entry->_recordfile ? entry->_recordfile : "";

    GHashTable *result = g_hash_table_new(NULL, g_str_equal);
    add_to_hashtable(result, ACCOUNT_ID_KEY, account_id);
    add_to_hashtable(result, CALLID_KEY, call_id);
    add_to_hashtable(result, CONFID_KEY, conf_id);
    add_to_hashtable(result, DISPLAY_NAME_KEY, display_name);
    add_to_hashtable(result, PEER_NUMBER_KEY, peer_number);
    add_to_hashtable(result, RECORDING_PATH_KEY, recording_path);
    add_to_hashtable(result, STATE_KEY, history_state);
    /* These values were already allocated dynamically */
    g_hash_table_insert(result, g_strdup(TIMESTAMP_START_KEY), time_start);
    g_hash_table_insert(result, g_strdup(TIMESTAMP_STOP_KEY), time_stop);
    return result;
}

gchar *get_formatted_start_timestamp(time_t start)
{
    time_t now = time(NULL);
    struct tm start_tm;

    localtime_r(&start, &start_tm);
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
    return g_markup_printf_escaped("%s\n", str);
}

gboolean call_was_outgoing(callable_obj_t * obj)
{
    return g_strcmp0(obj->_history_state, OUTGOING_STRING) == 0;
}

void restore_call(const gchar *id)
{
    g_debug("Restoring call %s", id);
    // We fetch the details associated to the specified call
    GHashTable *call_details = dbus_get_call_details(id);
    if (!call_details) {
        g_warning("Invalid call ID");
        return;
    }
    callable_obj_t *new_call = create_new_call_from_details(id, call_details);

    if (utf8_case_equal(g_hash_table_lookup(call_details, "CALL_TYPE"), INCOMING_STRING))
        new_call->_history_state = g_strdup(INCOMING_STRING);
    else
        new_call->_history_state = g_strdup(OUTGOING_STRING);

    calllist_add_call(current_calls_tab, new_call);
}
