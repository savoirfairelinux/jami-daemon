#!/usr/bin/env python
#
# Copyright (C) 2009 by the Free Software Foundation, Inc.
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

import time
import logging
from sflphonectrlsimple import SflPhoneCtrlSimple

from nose.tools import nottest

###
### function starting with 'test' are executed.
###

# Open sflphone and connect to sflphoned through dbus
sflphone = SflPhoneCtrlSimple(True)

accountList = ["IP2IP", "Account:1332798167"]

class TestSFLPhoneAccountConfig:

    def __init__(self):
        self.logger = logging.getLogger("TestSFLPhoneAccountConfig")
        filehdlr = logging.FileHandler("/tmp/sflphonedbustest.log")
        formatter = logging.Formatter("%(asctime)s %(levelname)s %(message)s")
        filehdlr.setFormatter(formatter)
        self.logger.addHandler(filehdlr)
        self.logger.setLevel(logging.INFO)

    @nottest
    def test_get_account_list(self):
        self.logger.info("Test get account list")
        accList = sflphone.getAllAccounts()
        listIntersection = set(accList) & set(accountList)
        assert len(listIntersection) == len(accountList)

    @nottest
    def test_account_registration(self):
        self.logger.info("Test account registration")
        accList = [x for x in sflphone.getAllAccounts() if x != "IP2IP"]
        for acc in accList:
	    self.logger.info("Registering account " + acc)

            if sflphone.isAccountEnable(acc):
               sflphone.setAccountEnable(acc, False)
               time.sleep(2)

            # Account should not be registered
            assert sflphone.isAccountRegistered(acc)

            sflphone.setAccountEnable(acc, True)
            time.sleep(2)

            assert sflphone.isAccountRegistered(acc)

    @nottest
    def test_add_remove_account(self):
        self.logger.info("Test add/remove account")
        accountDetails = {}
        newAccList = []

        # consider only true accounts
        accList = [x for x in sflphone.getAllAccounts() if x != "IP2IP"]

        # Store the account details localy
        for acc in accList:
            accountDetails[acc] = sflphone.getAccountDetails(acc)

        # Remove all accounts from sflphone
        for acc in accountDetails:
            sflphone.removeAccount(acc)

        # Recreate all accounts
        for acc in accountDetails:
            newAccList.append(sflphone.addAccount(accountDetails[acc]))

        # New accounts should be automatically registered
        for acc in newAccList:
            assert sflphone.isAccountRegistered(acc)
