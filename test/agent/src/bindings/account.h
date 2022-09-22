/*
 *  Copyright (C) 2021-2022 Savoir-faire Linux Inc.
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

#pragma once

#include "jami/configurationmanager_interface.h"
#include "jami/presencemanager_interface.h"

#include "utils.h"

static SCM details_binding(SCM accountID_str, SCM set_details_alist_optional)
{
    LOG_BINDING();

    auto details = DRing::getAccountDetails(from_guile(accountID_str));

    if (!SCM_UNBNDP(set_details_alist_optional)) {
	    DRing::setAccountDetails(from_guile(accountID_str),
				     from_guile(set_details_alist_optional));
    }

    return to_guile(details);
}

static SCM volatile_details_binding(SCM accountID_str)
{
    LOG_BINDING();

    return to_guile(DRing::getVolatileAccountDetails(from_guile(accountID_str)));
}

static SCM active_binding(SCM accountID_str, SCM active_boolean,
			  SCM shutdown_connections_boolean_optional)
{
    LOG_BINDING();

    bool shutdown_connections = false;

    if (!SCM_UNBNDP(shutdown_connections_boolean_optional)) {
	    shutdown_connections = from_guile(shutdown_connections_boolean_optional);
    }

    DRing::setAccountActive(from_guile(accountID_str),
			    from_guile(active_boolean),
			    from_guile(shutdown_connections));

    return SCM_UNDEFINED;
}

static SCM send_register_binding(SCM accountID_str, SCM enable_boolean)
{
    LOG_BINDING();

    DRing::sendRegister(from_guile(accountID_str),
                        from_guile(enable_boolean));

    return SCM_UNDEFINED;
}

static SCM export_to_file_binding(SCM accountID_str, SCM path_str, SCM passwd_str_optional)
{
    LOG_BINDING();

    if (SCM_UNBNDP(passwd_str_optional)) {
        return to_guile(DRing::exportToFile(from_guile(accountID_str),
                                            from_guile(path_str)));
    }

    return to_guile(DRing::exportToFile(from_guile(accountID_str),
                                        from_guile(path_str),
                                        from_guile(passwd_str_optional)));
}

static SCM
add_account_binding(SCM details_alist, SCM accountID_str_optional)
{
    LOG_BINDING();

    if (SCM_UNBNDP(accountID_str_optional)) {
        return to_guile(DRing::addAccount(from_guile(details_alist)));
    }

    return to_guile(DRing::addAccount(from_guile(details_alist),
                                      from_guile(accountID_str_optional)));
}

static SCM
remove_account_binding(SCM accountID_str)
{
    LOG_BINDING();

    DRing::removeAccount(from_guile(accountID_str));

    return SCM_UNDEFINED;

}

static SCM
accept_trust_request_binding(SCM accountID_str, SCM from_uri_str)
{
    LOG_BINDING();

    return to_guile(DRing::acceptTrustRequest(from_guile(accountID_str),
                                              from_guile(from_uri_str)));
}

static SCM
send_trust_request_binding(SCM accountID_str, SCM to_uri_str, SCM payload_vector_uint8_optional)
{
    LOG_BINDING();

    if (SCM_UNBNDP(payload_vector_uint8_optional)) {
        payload_vector_uint8_optional = scm_c_make_vector(0, SCM_UNDEFINED);
    }

    DRing::sendTrustRequest(from_guile(accountID_str),
                            from_guile(to_uri_str),
                            from_guile(payload_vector_uint8_optional));
    return SCM_UNDEFINED;
}

static SCM
get_contacts_binding(SCM accountID_str)
{
    LOG_BINDING();

    return to_guile(DRing::getContacts(from_guile(accountID_str)));
}

static SCM
subscribe_buddy_binding(SCM accountID_str, SCM peer_uri_str, SCM flag_bool)
{
    LOG_BINDING();

    DRing::subscribeBuddy(from_guile(accountID_str),
                          from_guile(peer_uri_str),
                          from_guile(flag_bool));

    return SCM_UNDEFINED;
}

static SCM
add_contact_binding(SCM accountID_str, SCM uri_str)
{
    LOG_BINDING();

    DRing::addContact(from_guile(accountID_str),
                      from_guile(uri_str));

    return SCM_UNDEFINED;
}

static SCM
remove_contact_binding(SCM accountID_str, SCM uri_str, SCM ban_optional_bool)
{
    LOG_BINDING();

    DRing::removeContact(from_guile(accountID_str),
                         from_guile(uri_str),
                         from_guile(ban_optional_bool));

    return SCM_UNDEFINED;
}


static void
install_account_primitives(void *)
{
    define_primitive("account-details", 1, 1, 0, (void*) details_binding);
    define_primitive("account-details/volatile", 1, 0, 0, (void*) details_volatile_binding);

    define_primitive("account-active", 2, 1, 0, (void*) active_binding);
    define_primitive("account-register", 2, 0, 0, (void*) send_register_binding);

    define_primitive("account-template", 1, 0, 0, (void*) account_template_binding);

    define_primitive("account->archive", 2, 1, 0, (void*) export_to_file_binding);

    define_primitive("add-account", 1, 1, 0, (void*) add_account_binding);
    define_primitive("remove-account", 1, 0, 0, (void*) remove_account_binding);
    define_primitive("list-accounts", 0, 0, 0, (void*) get_account_list_binding);

    define_primitive("lookup-name", 3, 0, 0, (void*) lookup_name_binding);
    define_primitive("lookup-address", 3, 0, 0, (void*) lookup_address_binding);
    define_primitive("register-name", 3, 0, 0, (void*) register_name_binding);
    define_primitive("search-user", 2, 0, 0, (void*) search_user_binding);

    define_primitive("accept-trust-request", 2, 0, 0, (void*) accept_trust_request_binding);
    define_primitive("discard-trust-request", 2, 0, 0, (void*) discard_trust_request_binding);
    define_primitive("send-trust-request", 2, 1, 0, (void*) send_trust_request_binding);

    define_primitive("add-contact", 2, 0, 0, (void*) add_contact_binding);
    define_primitive("remove-contact", 2, 1, 0, (void*) remove_contact_binding);
    define_primitive("list-contacts", 1, 0, 0, (void*) get_contacts_binding);
    define_primitive("get-contact-details", 2, 0, 0, (void*) get_contact_details_binding);

    define_primitive("subscribe-buddy", 3, 0, 0, (void*) subscribe_buddy_binding);
    define_primitive("publish", 3, 0, 0, (void*) publish_binding);
}
