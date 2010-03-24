/*
 *  Copyright (C) 2004-2007 Savoir-Faire Linux inc.
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
 */

#include <cppunit/extensions/HelperMacros.h>
#include <map>
#include "accounttest.h"
#include "manager.h"
#include "logger.h"
#include "validator.h"

void AccountTest::TestAddRemove(void) {
	_debug ("-------------------- AccountTest::TestAddRemove --------------------\n");

	std::map<std::string, std::string> details;
	details[CONFIG_ACCOUNT_TYPE] = "SIP";
	details[CONFIG_ACCOUNT_ENABLE] = "false";

	std::string accountId = Manager::instance().addAccount(details);
	CPPUNIT_ASSERT(Validator::isNotNull(accountId));
	CPPUNIT_ASSERT(Manager::instance().accountExists(accountId));

	Manager::instance().removeAccount(accountId);

	CPPUNIT_ASSERT(!Manager::instance().accountExists(accountId));
}
