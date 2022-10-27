#!/usr/bin/env python
#
# Copyright (C) 2012 by the Free Software Foundation, Inc.
#
# Author: Alexandre Savard <alexandre.savard@savoirfairelinux.com>
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
import yaml
import logging
import multiprocessing
from sippwrap import SippWrapper
from sippwrap import SippScreenStatParser
from jamictrl import libjamiCtrl as SflPhoneCtrl

from nose.tools import nottest

###
### function starting with 'test' are executed.
###

SCENARIO_PATH = "../sippxml/"

class SippCtrl:

    def __init__(self):
        self.remoteServer = "127.0.0.1"
        self.remotePort = str(5062)
        self.localInterface = "127.0.0.1"
        self.localPort = str(5060)

    def initialize_sipp_registration_instance(self, instance, xmlScenario):
        instance.remoteServer = self.remoteServer
        instance.remotePort = self.remotePort
        instance.localInterface = self.localInterface
        instance.localPort = self.localPort
        instance.customScenarioFile = SCENARIO_PATH + xmlScenario
        instance.numberOfCall = 1
        instance.numberOfSimultaneousCall = 1

    def initialize_sipp_call_instance(self, instance):
        instance.localInterface = self.localInterface
        instance.localPort = self.localPort
        instance.numberOfCall = 1
        instance.numberOfSimultaneousCall = 1
        instance.enableTraceScreen = True

    def launchSippProcess(self, sippInstance, localPort):
        sippInstance.buildCommandLine(localPort)
        sippInstance.launch()

    def find_sipp_pid(self):
        # Retrieve the PID of the last
        # The /proc/PID/cmdline contain the command line from
        pids = [int(x) for x in os.listdir("/proc") if x.isdigit()]
        sippPid = [pid for pid in pids if "sipp" in open("/proc/" + str(pid) + "/cmdline").readline()]

        return sippPid[0]

    def clean_log_directory(self):
        dirlist = os.listdir("./")
        files = [x for x in dirlist if "screen.log" in x]
        for f in files:
	    os.remove(f)

    def parse_results(self):
        dirlist = os.listdir("./")
        logfile = [x for x in dirlist if "screen.log" in x]

        fullpath = os.path.dirname(os.path.realpath(__file__)) + "/"

        # there should be only one screen.log file (see clean_log_directory)
        resultParser = SippScreenStatParser(fullpath + logfile[0])

        assert(not resultParser.isAnyFailedCall())
        assert(resultParser.isAnySuccessfulCall())

class TestSFLPhoneAccountConfig(SflPhoneCtrl):
    """ The test suite for account configuration """

    def __init__(self):
        SflPhoneCtrl.__init__(self, "test", False)

        self.logger = logging.getLogger("TestSFLPhoneAccountConfig")
        filehdlr = logging.FileHandler("/tmp/sflphonedbustest.log")
        formatter = logging.Formatter("%(asctime)s %(levelname)s %(message)s")
        filehdlr.setFormatter(formatter)
        self.logger.addHandler(filehdlr)
        self.logger.setLevel(logging.INFO)


    def get_config(self):
        """ Parsse configuration file and return a dictionary """
        config = {}
        with open("sflphoned.functest.yml","r") as stream:
            config = yaml.load(stream)

        return config


    def get_account_list_from_config(self):
        """ Get the accout list from config and add IP2IP """

        config = self.get_config()

        accounts = config["preferences"]["order"]
        accountList = accounts.split('/')
        del accountList[len(accountList)-1]
        accountList.append("IP2IP")

        return accountList


    def test_get_account_list(self):
        self.logger.info("Test get account list")

        accountList = self.get_account_list_from_config()

        # make sure that the intersection between the list is of same size
        accList = self.getAllAccounts()
        listIntersection = set(accList) & set(accountList)
        assert len(listIntersection) == len(accountList)


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
    def test_get_account_details(self):
        self.logger.info("Test account details")

        accList = [x for x in self.getAllAccounts() if x != "IP2IP"]

        config = self.get_config()

        accountDetails = {}
        for acc in accList:
            accountDetails[acc] = self.getAccountDetails(acc)

        accountConfDetails = {}
        for accConf in config["accounts"]:
            accountConfDetails[accConf["id"]] = accConf


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



class TestSFLPhoneRegisteredCalls(SflPhoneCtrl, SippCtrl):
    """ The test suite for call interaction """

    def __init__(self):
        SflPhoneCtrl.__init__(self, "test", False)
        SippCtrl.__init__(self)

        self.logger = logging.getLogger("TestSFLPhoneRegisteredCalls")
        filehdlr = logging.FileHandler("/tmp/sfltestregisteredcall.log")
        formatter = logging.Formatter("%(asctime)s %(levelname)s %(message)s")
        filehdlr.setFormatter(formatter)
        self.logger.addHandler(filehdlr)
        self.logger.setLevel(logging.INFO)
        self.sippRegistrationInstance = SippWrapper()
        self.sippCallInstance = SippWrapper()

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


    def test_registered_call(self):
        self.logger.info("Test Registered Call")

        # Launch a sipp instance for account registration on asterisk
        # this account will then be used to receive call from sflphone
        self.initialize_sipp_registration_instance(self.sippRegistrationInstance, "uac_register_no_cvs_300.xml")
        regd = multiprocessing.Process(name='sipp1register', target=self.launchSippProcess,
                                                          args=(self.sippRegistrationInstance, 5064,))
        regd.start()

        # wait for the registration to complete
        regd.join()

        # Launch a sipp instance waiting for a call from previously registered account
        self.initialize_sipp_call_instance(self.sippCallInstance)
        calld = multiprocessing.Process(name='sipp1call', target=self.launchSippProcess,
                                                      args=(self.sippCallInstance, 5064,))
        calld.start()

        # Make sure every account are enabled
        accList = [x for x in self.getAllAccounts() if x != "IP2IP"]
        for acc in accList:
            if not self.isAccountRegistered(acc):
                self.setAccountEnable(acc, True)

        # Make a call to the SIPP instance
        self.Call("300")

        # Start the threaded loop to handle GLIB cllbacks
        self.start()

        # Wait for the sipp instance to dump log files
        calld.join()

        self.stopThread()
        self.parse_results()


class TestSFLPhoneConferenceCalls(SflPhoneCtrl, SippCtrl):
    """ Test Conference calls """

    def __init__(self):
        SflPhoneCtrl.__init__(self, "test", False)
        SippCtrl.__init__(self)

        self.logger = logging.getLogger("TestSFLPhoneRegisteredCalls")
        filehdlr = logging.FileHandler("/tmp/sfltestregisteredcall.log")
        formatter = logging.Formatter("%(asctime)s %(levelname)s %(message)s")
        filehdlr.setFormatter(formatter)
        self.logger.addHandler(filehdlr)
        self.logger.setLevel(logging.INFO)
        self.sippRegistrationInstanceA = SippWrapper()
        self.sippRegistrationInstanceB = SippWrapper()
        self.sippCallInstanceA = SippWrapper()
        self.sippCallInstanceB = SippWrapper()
        self.localPortCallA = str(5064)
        self.localPortCallB = str(5066)
        self.callCount = 0
        self.accountCalls = []

        # Make sure the test directory is populated with most recent log files
        # self.clean_log_directory()


    def onCallCurrent_cb(self):
        """ On incoming call, answer the call, then hangup """

        self.callCount += 1

        self.accountCalls.append(self.currentCallId)
        print "Account List: ", str(self.accountCalls)

        if self.callCount == 2:
            self.createConference(self.accountCalls[0], self.accountCalls[1])


    def onCallRinging_cb(self):
        """ Display messages when call is ringing """

        print "The call is ringing"


    def onCallHangup_cb(self, callId):
        """ Exit thread when all call are finished """

        if callId in self.accountCalls:
            self.accountCalls.remove(callId)

            self.callCount -= 1
            if self.callCount == 0:
                self.stopThread()


    def onCallFailure_cb(self):
        """ If a failure occurs duing the call, just leave the running thread """

        print "Stopping Thread"
        self.stopThread()


    def onConferenceCreated_cb(self):
        """ Called once the conference is created """

        print "Conference Created ", self.currentConfId
        print "Conference Hangup ", self.currentConfId

        self.hangupConference(self.currentConfId)


    def test_conference_call(self):
        self.logger.info("Test Registered Call")

        # launch the sipp instance to register the first participant to astersik
        self.initialize_sipp_registration_instance(self.sippRegistrationInstanceA, "uac_register_no_cvs_300.xml")
        regd = multiprocessing.Process(name='sipp1register', target=self.launchSippProcess,
                                                                 args=(self.sippRegistrationInstanceA, 5064,))
        regd.start()
        regd.join()

        # launch the sipp instance to register the second participant to asterisk
        self.initialize_sipp_registration_instance(self.sippRegistrationInstanceB, "uac_register_no_cvs_400.xml")
        regd = multiprocessing.Process(name='sipp2register', target=self.launchSippProcess,
                                                                 args=(self.sippRegistrationInstanceB, 5066,))
        regd.start()
        regd.join()

        # launch the sipp instance waining for call as the first participant
        self.initialize_sipp_call_instance(self.sippCallInstanceA)
        calldA = multiprocessing.Process(name='sipp1call', target=self.launchSippProcess,
                                                              args=(self.sippCallInstanceA, 5064,))
        calldA.start()


        # launch the sipp instance waiting for call as the second particpant
        self.initialize_sipp_call_instance(self.sippCallInstanceB)
        calldB = multiprocessing.Process(name='sipp2call', target=self.launchSippProcess,
                                                               args=(self.sippCallInstanceB, 5066,))
        calldB.start()

        # make sure every account are enabled
        accList = [x for x in self.getAllAccounts() if x != "IP2IP"]
        for acc in accList:
            if not self.isAccountRegistered(acc):
                self.setAccountEnable(acc, True)

        # make a call to the SIPP instance
        self.Call("300")
        self.Call("400")

        # start the main loop for processing glib callbacks
        self.start()

        print "-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-"
        calldA.join()
        print "+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+"
        calldB.join()

        print "====================================================="

        self.stopThread()
        self.parse_results()


# callInstance = TestSFLPhoneRegisteredCalls()
# callInstance.test_registered_call()

confInstance = TestSFLPhoneConferenceCalls()
confInstance.test_conference_call()
