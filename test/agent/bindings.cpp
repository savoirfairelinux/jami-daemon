/*
 *  Copyright (C) 2021 Savoir-faire Linux Inc.
 *
 *  Author: Olivier Dion <olivier.dion@savoirfairelinux.com>
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
 */

#include "agent/agent.h"
#include "agent/bindings.h"

static inline SCM
to_guile(bool b)
{
    return scm_from_bool(b);
}

static inline SCM
to_guile(const std::string& str)
{
    return scm_from_utf8_string(str.c_str());
}

template<typename T>
static inline SCM
to_guile(const std::vector<T>& values)
{
    SCM vec = scm_make_vector(values.size(), SCM_UNDEFINED);

    for (size_t i = 0; i < values.size(); ++i) {
        SCM_SIMPLE_VECTOR_SET(vec, i, to_guile(values[i]));
    }

    return vec;
}

template<typename K, typename V>
static inline SCM
to_guile(const std::map<K, V>& map)
{
    SCM assoc = SCM_EOL;

    for (auto const& [key, value] : map) {
        SCM pair = scm_cons(to_guile(key), to_guile(value));
        assoc = scm_cons(pair, assoc);
    }

    return assoc;
}

template<typename... Args>
static inline SCM
pack_to_guile(Args... args)
{
    SCM lst = SCM_EOL;
    std::vector<SCM> values = {to_guile(args)...};

    while (values.size()) {
        lst = scm_cons(values.back(), lst);
        values.pop_back();
    }

    return lst;
}

template<typename... Args>
static inline SCM
apply_to_guile(SCM body_proc, Args... args)
{
    AGENT_ASSERT(scm_is_true(scm_procedure_p(body_proc)),
                 "body_proc must be a procedure");

    SCM arglst = pack_to_guile(args...);

    return scm_apply_0(body_proc, arglst);
}

struct from_guile
{
    SCM value;

    from_guile(SCM val)
        : value(val)
    {}

    operator bool()
    {
        AGENT_ASSERT(scm_is_bool(value), "Scheme value must be of type bool");

        return scm_to_bool(value);
    }

    operator std::string()
    {
        AGENT_ASSERT(scm_is_string(value), "Scheme value must be of type string");

        char* str_raw = scm_to_locale_string(value);
        std::string ret(str_raw);
        free(str_raw);

        return ret;
    }

    template<typename T>
    operator std::vector<T>()
    {
        AGENT_ASSERT(scm_is_simple_vector(value), "Scheme value must be a simple vector");

        std::vector<T> ret;

        for (size_t i = 0; i < SCM_SIMPLE_VECTOR_LENGTH(value); ++i) {
            SCM val = SCM_SIMPLE_VECTOR_REF(value, i);

            ret.emplace_back(from_guile(val));
        }

        return ret;
    }

    template<typename K, typename V>
    operator std::map<K, V>()
    {
        AGENT_ASSERT(scm_is_true(scm_list_p(value)), "Scheme value mut be a list");

        std::map<K, V> ret;

        while (not scm_is_null(value)) {
            SCM pair = scm_car(value);

            K key = from_guile(scm_car(pair));
            V val = from_guile(scm_cdr(pair));

            ret[key] = val;

            value = scm_cdr(value);
        }

        return ret;
    }
};

static SCM
wait_binding(SCM period_long_optional)
{
    std::chrono::seconds period;

    if (SCM_UNBNDP(period_long_optional)) {
        period = std::chrono::seconds::max();
    } else {
        AGENT_ASSERT(scm_is_number(period_long_optional), "Invalid period");
        period = std::chrono::seconds(scm_to_uint(period_long_optional));
    }

    Agent::instance().wait(period);

    return SCM_UNDEFINED;
}

static SCM
place_call_binding(SCM contact_str)
{
    return to_guile(Agent::instance().placeCall(from_guile(contact_str)));
}

static SCM
some_conversation_binding()
{
    return to_guile(Agent::instance().someConversation());
}

static SCM
some_contact_binding()
{
    return to_guile(Agent::instance().someContact());
}

static SCM
search_peer_binding(SCM peers_vector_or_str)
{
    std::vector<std::string> peers;

    if (scm_is_string(peers_vector_or_str)) {
        peers.emplace_back(from_guile(peers_vector_or_str));
    } else {
        peers = from_guile(peers_vector_or_str);
    }

    Agent::instance().searchForPeers(peers);

    return SCM_UNDEFINED;
}

static SCM
ping_binding(SCM contact_str)
{
    return to_guile(Agent::instance().ping(from_guile(contact_str)));
}

static SCM
set_details_binding(SCM details_alist)
{
    Agent::instance().setDetails(from_guile(details_alist));

    return SCM_UNDEFINED;
}

static SCM
ensure_account_binding()
{
    Agent::instance().ensureAccount();

    return SCM_UNDEFINED;
}

static SCM
export_to_archive_binding(SCM path)
{
    Agent::instance().exportToArchive(from_guile(path));

    return SCM_UNDEFINED;
}

static SCM
import_from_archive_binding(SCM path)
{
    Agent::instance().importFromArchive(from_guile(path));

    return SCM_UNDEFINED;
}

static SCM
enable_binding()
{
    Agent::instance().activate(true);

    return SCM_UNDEFINED;
}

static SCM
disable_binding()
{
    Agent::instance().activate(false);

    return SCM_UNDEFINED;
}

static SCM
wait_event_binding()
{
    Agent::instance().waitForEvent();

    return SCM_UNDEFINED;
}

/*
 * Register Guile bindings here.
 *
 * 1. Name of the binding
 * 2. Number of required argument to binding
 * 3. Number of optional argument to binding
 * 4. Number of rest argument to binding
 * 5. Pointer to C function to call
 *
 * See info guile:
 *
 * Function: SCM scm_c_define_gsubr(const char *name, int req, int opt, int rst, fcn):
 *
 * Register a C procedure FCN as a “subr” — a primitive subroutine that can be
 * called from Scheme.  It will be associated with the given NAME and bind it in
 * the "current environment".  The arguments REQ, OPT and RST specify the number
 * of required, optional and “rest” arguments respectively.  The total number of
 * these arguments should match the actual number of arguments to FCN, but may
 * not exceed 10.  The number of rest arguments should be 0 or 1.
 * ‘scm_c_make_gsubr’ returns a value of type ‘SCM’ which is a “handle” for the
 * procedure.
 */
void
install_scheme_primitives()
{
    auto define_primitive = [](const char* name, int req, int opt, int rst, void* func) {
        AGENT_ASSERT(req + opt + rst <= 10, "Primitive binding `%s` has too many argument", name);

        AGENT_ASSERT(0 == rst or 1 == rst, "Rest argument for binding `%s` must be 0 or 1", name);

        scm_c_define_gsubr(name, req, opt, rst, func);
    };

    define_primitive("agent:search-for-peers", 1, 0, 0, (void*) search_peer_binding);
    define_primitive("agent:place-call", 1, 0, 0, (void*) place_call_binding);
    define_primitive("agent:some-contact", 0, 0, 0, (void*) some_contact_binding);
    define_primitive("agent:some-conversation", 0, 0, 0, (void*) some_conversation_binding);
    define_primitive("agent:wait", 0, 1, 0, (void*) wait_binding);
    define_primitive("agent:ping", 1, 0, 0, (void*) ping_binding);
    define_primitive("agent:set-details", 1, 0, 0, (void*) set_details_binding);
    define_primitive("agent:ensure-account", 0, 0, 0, (void*) ensure_account_binding);
    define_primitive("agent->archive", 1, 0, 0, (void*) export_to_archive_binding);
    define_primitive("archive->agent", 1, 0, 0, (void*) import_from_archive_binding);
    define_primitive("agent:enable", 0, 0, 0, (void*) enable_binding);
    define_primitive("agent:disable", 0, 0, 0, (void*) disable_binding);
    define_primitive("agent:wait-event", 0, 0, 0, (void*) wait_event_binding);
}
