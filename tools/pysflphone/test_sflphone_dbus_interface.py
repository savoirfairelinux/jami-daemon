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

import os
import time
import logging
from sippwrap import SippWrapper
from sippwrap import SippScreenStatParser
from sflphonectrl import SflPhoneCtrl

from nose.tools import nottest

###
### function starting with 'test' are executed.
###

accountList = ["IP2IP", "Account:1332798167"]

SCENARIO_PATH = "../sippxml/"

class TestSFLPhoneAccountConfig(SflPhoneCtrl):
    """ The test suite for account configuration """

    def __init__(self):
        SflPhoneCtrl.__init__(self)

        self.logger = logging.getLogger("TestSFLPhoneAccountConfig")
        filehdlr = logging.FileHandler("/tmp/sflphonedbustest.log")
        formatter = logging.Formatter("%(asctime)s %(levelname)s %(message)s")
        filehdlr.setFormatter(formatter)
        self.logger.addHandler(filehdlr)
        self.logger.setLevel(logging.INFO)

    @nottest
    def test_get_account_list(self):
        self.logger.info("Test get account list")
        accList = self.getAllAccounts()
        listIntersection = set(accList) & set(accountList)
        assert len(listIntersection) == len(accountList)

    @nottest
    def test_account_registration(self):
        self.logger.info("Test account registration")
        accList = [x for x in self.getAllAccounts() if x != "IP2IP"]
        for acc in accList:
	    self.logger.info("Registering account " + acc)

            if self.isAccountEnable(acc):
               self.setAccountEnable(acc, False)
               time.sleep(2)

            # Account should not be registered
            assert self.isAccountRegistered(acc)

            self.setAccountEnable(acc, True)
            time.sleep(2)

            assert self.isAccountRegistered(acc)

    @nottest
    def test_add_remove_account(self):
        self.logger.info("Test add/remove account")
        accountDetails = {}
        newAccList = []

        # consider only true accounts
        accList = [x for x in self.getAllAccounts() if x != "IP2IP"]

        # Store the account details localy
        for acc in accList:
            accountDetails[acc] = self.getAccountDetails(acc)

        # Remove all accounts from sflphone
        for acc in accountDetails:
            self.removeAccount(acc)

        # Recreate all accounts
        for acc in accountDetails:
            newAccList.append(self.addAccount(accountDetails[acc]))

        # New accounts should be automatically registered
        for acc in newAccList:
            assert self.isAccountRegistered(acc)



class TestSFLPhoneRegisteredCalls(SflPhoneCtrl):
    """ The test suite for call interaction """

    def __init__(self):
        SflPhoneCtrl.__init__(self)

        self.logger = logging.getLogger("TestSFLPhoneRegisteredCalls")
        filehdlr = logging.FileHandler("/tmp/sfltestregisteredcall.log")
        formatter = logging.Formatter("%(asctime)s %(levelname)s %(message)s")
        filehdlr.setFormatter(formatter)
        self.logger.addHandler(filehdlr)
        self.logger.setLevel(logging.INFO)
        self.sippRegistrationInstance = SippWrapper()
        self.sippCallInstance = SippWrapper()
        self.localInterface = "127.0.0.1"
        self.localPort = str(5064)

        # Make sure the test directory is populated with most recent log files
        self.clean_log_directory()


    def onCallCurrent_cb(self):
        """ On incoming call, answer the callm, then hangup """

        print "Hangup Call with id " + self.currentCallId
        self.HangUp(self.currentCallId)

        print "Stopping Thread"
        self.stopThread()


    def onCallRinging_cb(self):
        """ Display messages when call is ringing """

        print "The call is ringing"


    def onCallFailure_cb(self):
        """ If a failure occurs duing the call, just leave the running thread """

        print "Stopping Thread"
        self.stopThread()


    def clean_log_directory(self):
        dirlist = os.listdir("./")
        files = [x for x in dirlist if "screen.log" in x]
        for f in files:
	    os.remove(f)


    def find_sipp_pid(self):
        # Retreive the PID of the last
        # The /proc/PID/cmdline contain the command line from
        pids = [int(x) for x in os.listdir("/proc") if x.isdigit()]
        sippPid = [pid for pid in pids if "sipp" in open("/proc/" + str(pid) + "/cmdline").readline()]

        return sippPid[0]


    def parse_results(self):
        dirlist = os.listdir("./")
        logfile = [x for x in dirlist if "screen.log" in x]
        print logfile

        fullpath = os.path.dirname(os.path.realpath(__file__)) + "/"

        # there should be only one screen.log file (see clean_log_directory)
        resultParser = SippScreenStatParser(fullpath + logfile[0])

        assert(not resultParser.isAnyFailedCall())
        assert(resultParser.isAnySuccessfulCall())


    def test_registered_call(self):
        self.logger.info("Test Registered Call")

        # launch the sipp instance in background
        # sipp 127.0.0.1:5060 -sf uac_register_no_cvs.xml -i 127.0.0.1 -p 5062
        self.sippRegistrationInstance.remoteServer = "127.0.0.1"
        self.sippRegistrationInstance.remotePort = str(5062)
        self.sippRegistrationInstance.localInterface = self.localInterface
        self.sippRegistrationInstance.localPort = self.localPort
        self.sippRegistrationInstance.customScenarioFile = SCENARIO_PATH + "uac_register_no_cvs.xml"
        self.sippRegistrationInstance.launchInBackground = True
        self.sippRegistrationInstance.numberOfCall = 1
        self.sippRegistrationInstance.numberOfSimultaneousCall = 1

        self.sippRegistrationInstance.launch()

        # wait for this instance of sipp to complete registration
        sippPid = self.find_sipp_pid()
        while os.path.exists("/proc/" + str(sippPid)):
            time.sleep(1)

        # sipp -sn uas -p 5062 -i 127.0.0.1
        self.sippCallInstance.localInterface = self.localInterface
        self.sippCallInstance.localPort = self.localPort
        self.sippCallInstance.launchInBackground = True
        self.sippCallInstance.numberOfCall = 1
        self.sippCallInstance.numberOfSimultaneousCall = 1
        self.sippCallInstance.enableTraceScreen = True

        self.sippCallInstance.launch()

        sippPid = self.find_sipp_pid()

        # make sure every account are enabled
        accList = [x for x in self.getAllAccounts() if x != "IP2IP"]
        for acc in accList:
            if not self.isAccountRegistered(acc):
                self.setAccountEnable(acc, True)

        # Make a call to the SIPP instance
        self.Call("300")

        # Start Glib mainloop to process callbacks
        self.start()

        # Wait the sipp instance to dump log files
        while os.path.exists("/proc/" + str(sippPid)):
            time.sleep(1)

        self.parse_results()
