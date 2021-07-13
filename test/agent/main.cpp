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

/* agent */
#include "agent/agent.h"
#include "agent/log.h"

/* Jami */
#include "dring.h"

/* Third parties */
#include <gnutls/gnutls.h>
#include <libguile.h>

/* std */
#include <filesystem>
#include <fstream>

/* Guile helpers */

#define scm_to_cxx_string(VAR) (scm_to_cxx_string)(VAR, #VAR)

static std::string(scm_to_cxx_string)(SCM value_str, const char* value_name)
{
    AGENT_ASSERT(scm_is_string(value_str), "`%s` must be of type string", value_name);

    char* str_raw = scm_to_locale_string(value_str);
    std::string ret(str_raw);
    free(str_raw);

    return ret;
}

/* Begin bindings here */

static SCM
wait_binding(SCM period_long)
{
    AGENT_ASSERT(scm_is_number(period_long), "Invalid period");

    Agent::instance().wait(std::chrono::seconds(scm_to_uint(period_long)));

    return SCM_UNDEFINED;
}

static SCM
place_call_binding(SCM contact_str)
{
    AGENT_ASSERT(scm_is_string(contact_str), "Wrong type for contact");

    return scm_from_bool(Agent::instance().placeCall(scm_to_cxx_string(contact_str)));
}

static SCM
some_conversation_binding()
{
    auto contact = Agent::instance().someConversation();

    return scm_from_utf8_string(contact.c_str());
}

static SCM
some_contact_binding()
{
    auto contact = Agent::instance().someContact();

    return scm_from_utf8_string(contact.c_str());
}

static SCM
search_peer_binding(SCM peers_vector_or_str)
{
    std::vector<std::string> peers;

    if (scm_is_string(peers_vector_or_str)) {
        peers.emplace_back(scm_to_cxx_string(peers_vector_or_str));
    } else {
        AGENT_ASSERT(scm_is_simple_vector(peers_vector_or_str),
                     "peers_vector_or_str must be a simple vector or a string");
    }

    for (size_t i = 0; i < SCM_SIMPLE_VECTOR_LENGTH(peers_vector_or_str); ++i) {
        SCM peer_str = SCM_SIMPLE_VECTOR_REF(peers_vector_or_str, i);

        peers.emplace_back(scm_to_cxx_string(peer_str));
    }

    Agent::instance().searchForPeers(peers);

    return SCM_UNDEFINED;
}

static SCM
ping_binding(SCM contact_str)
{
    return scm_from_bool(Agent::instance().ping(scm_to_cxx_string(contact_str)));
}

static SCM
install_file_logger_binding(SCM context_str, SCM to_str)
{
    AGENT_ASSERT(scm_is_string(context_str), "Context must be a string");
    AGENT_ASSERT(scm_is_string(to_str), "To must be a string");

    std::string context = scm_to_cxx_string(context_str);
    std::string to = scm_to_cxx_string(to_str);

    Agent::instance().installLogHandler(makeLogHandler<FileHandler>(context, to));

    return SCM_UNDEFINED;
}

static SCM
install_net_logger_binding(SCM context_str, SCM to_str, SCM port_uint16)
{
    AGENT_ASSERT(scm_is_string(context_str), "Context must be a string");
    AGENT_ASSERT(scm_is_string(to_str), "To must be a string");
    AGENT_ASSERT(scm_is_number(port_uint16), "Port must be a number");

    std::string context = scm_to_cxx_string(context_str);
    std::string to = scm_to_cxx_string(to_str);

    Agent::instance().installLogHandler(
        makeLogHandler<NetHandler>(context, to, scm_to_uint16(port_uint16)));

    return SCM_UNDEFINED;
}

static SCM
remove_logger_binding()
{
    Agent::instance().removeLogHandler();

    return SCM_UNDEFINED;
}

static SCM
set_details_binding(SCM details_alist)
{
    std::map<std::string, std::string> details;

    AGENT_ASSERT(scm_is_true(scm_list_p(details_alist)), "Bad format of details");

    while (not scm_is_null(details_alist)) {
        SCM detail_pair = scm_car(details_alist);

        AGENT_ASSERT(scm_is_pair(detail_pair), "Detail must be a pair");

        auto car = scm_to_cxx_string(scm_car(detail_pair));
        auto cdr = scm_to_cxx_string(scm_cdr(detail_pair));

        details[car] = cdr;

        details_alist = scm_cdr(details_alist);
    }

    Agent::instance().setDetails(details);

    return SCM_UNDEFINED;
}

/* End bindings here */

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
static void
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
    define_primitive("agent:wait", 1, 0, 0, (void*) wait_binding);
    define_primitive("agent:ping", 1, 0, 0, (void*) ping_binding);
    define_primitive("agent:install-file-logger", 2, 0, 0, (void*) install_file_logger_binding);
    define_primitive("agent:install-net-logger", 3, 0, 0, (void*) install_net_logger_binding);
    define_primitive("agent:remove-logger", 0, 0, 0, (void*) remove_logger_binding);
    define_primitive("agent:set-details", 1, 0, 0, (void*) set_details_binding);
}

static SCM
main_body(void* buf)
{
    scm_c_eval_string((char*) buf);

    return SCM_UNDEFINED;
}

static SCM
main_catch(void* nil, SCM key_sym, SCM rest_lst)
{
    (void) nil;

    SCM fmt_str = scm_from_utf8_string("Guile exception `~a`: ~a");
    SCM args_lst = scm_list_2(key_sym, rest_lst);
    SCM to_print_str = scm_simple_format(SCM_BOOL_F, fmt_str, args_lst);

    char* to_print_raw = scm_to_locale_string(to_print_str);

    AGENT_ERR("%s\n", to_print_raw);

    free(to_print_raw);

    return SCM_UNDEFINED;
}

void*
main_inner(void* agent_config_raw) /* In Guile context */
{
    const char* agent_config = (const char*) (agent_config_raw);

    std::vector<char> buf;
    size_t len;

    {
        std::ifstream file = std::ifstream((char*) agent_config);

        AGENT_ASSERT(file.is_open(), "Can't open file %s", agent_config);

        file.seekg(0, file.end);
        len = file.tellg();
        file.seekg(0, file.beg);

        buf.reserve(len + 1);
        file.read(buf.data(), len);
        buf[len] = '\0';
    }

    install_scheme_primitives();

    Agent::instance().init();

    scm_internal_catch(SCM_BOOL_T, main_body, buf.data(), main_catch, nullptr);

    Agent::instance().fini();

    return nullptr;
}

int
main(int argc, char* argv[])
{
    if (argc < 2) {
        printf("Usage: agent CONFIG\n");
        exit(EXIT_FAILURE);
    }

    setenv("GUILE_LOAD_PATH", std::filesystem::current_path().c_str(), 1);

    /* NOTE!  It's very important to initialize the daemon before entering Guile!!! */
    DRing::init(DRing::InitFlag(DRing::DRING_FLAG_DEBUG | DRing::DRING_FLAG_CONSOLE_LOG));

    AGENT_ASSERT(DRing::start(""), "Failed to start daemon");

    /* Entering guile context */
    scm_with_guile(main_inner, argv[1]);

    DRing::fini();
}
