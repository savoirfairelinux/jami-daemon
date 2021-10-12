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

#pragma once

/* Jami */
#include "jami/callmanager_interface.h"
#include "jami/configurationmanager_interface.h"

/* Agent */
#include "utils.h"

static SCM set_details_binding(SCM accountID_str, SCM details_alist)
{
	DRing::setAccountDetails(from_guile(accountID_str),
				 from_guile(details_alist));
	return SCM_UNDEFINED;
}

static SCM get_details_binding(SCM accountID_str)
{
	return to_guile(DRing::getAccountDetails(from_guile(accountID_str)));
}

static SCM send_register_binding(SCM accountID_str, SCM enable_boolean)
{
	DRing::sendRegister(from_guile(accountID_str),
			    from_guile(enable_boolean));

	return SCM_UNDEFINED;
}

static SCM export_to_file_binding(SCM accountID_str, SCM path_str, SCM passwd_str_optional)
{
	if (SCM_UNBNDP(passwd_str_optional)) {
		return to_guile(DRing::exportToFile(from_guile(accountID_str),
						    from_guile(path_str)));
	}

	return to_guile(DRing::exportToFile(from_guile(accountID_str),
					    from_guile(path_str),
					    from_guile(passwd_str_optional)));
}

static SCM add_account_binding(SCM details_alist, SCM accountID_str_optional)
{
	if (SCM_UNBNDP(accountID_str_optional)) {
		return to_guile(DRing::addAccount(from_guile(details_alist)));
	}

	return to_guile(DRing::addAccount(from_guile(details_alist),
					  from_guile(accountID_str_optional)));
}

static void
install_account_primitives(void *)
{
    define_primitive("set-details", 2, 0, 0, (void*) set_details_binding);
    define_primitive("get-details", 1, 0, 0, (void*) get_details_binding);
    define_primitive("send-register", 2, 0, 0, (void*) send_register_binding);
    define_primitive("account->archive", 2, 1, 0, (void*) export_to_file_binding);
    define_primitive("add", 1, 1, 0, (void*) add_account_binding);
}
