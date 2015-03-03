#!/usr/bin/env python3
#
# Copyright (C) 2015 Savoir-Faire Linux Inc.
# Author: Eloi Bail <eloi.bail@savoirfairelinux.com>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301 USA.
#
# Additional permission under GNU GPL version 3 section 7:
#
# If you modify this program, or any covered work, by linking or
# combining it with the OpenSSL project's OpenSSL library (or a
# modified version of that library), containing parts covered by the
# terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
# grants you additional permission to convey the resulting work.
# Corresponding Source for a non-source form of such a combination
# shall include the source code for the parts of OpenSSL used as well
# as that of the covered work.
#

import sys
import os
import time
from threading import Thread
from random import shuffle
from errors import *


#Dht Client
SIP_TEST_ACCOUNT = 'sf1'
#Dht Client
DHT_TEST_ACCOUNT = '6c8d591e7c55b348338209bb6f9f638688d50034'
#Ring Client
RING_TEST_ACCOUNT = '192.168.48.125'
#RING_TEST_ACCOUNT = '192.168.48.158'
#Polycom Client
POLYCOM_TEST_ACCOUNT = '192.168.40.38'

WITH_HOLD = True;
WITHOUT_HOLD = False;

class DRingTester():
    def testConfig(self, ctrl):
        print("**[BEGIN] test config");
        allCodecs = ctrl.getAllCodecs()
        if len(allCodecs)==0:
            print(("error no codec on the system"))
            return 0
        print("**[END] test config");
        return 1

    def checkIP2IPAccount(self, ctrl):
        ipAccount = ctrl.getAllAccounts()
        print((ipAccount))
        if len(ipAccount)==0:
            print(("no IP2IP account"))
            return 0
        return 1

    def registerAccount(self, ctrl, account):
        print("registering:"+account)
        ctrl.setAccountRegistered(account, True)


    def setRandomActiveCodecs(self, ctrl, account):
        codecList = ctrl.getAllCodecs()
        shuffle(codecList)
        ctrl.setActiveCodecList(account,str(codecList)[1 : -1])
        print("New active list for "+account)
        print(ctrl.getActiveCodecs(account))

    def holdToggle(self, ctrl, callId, delay):
            time.sleep(delay)
            print("Holding: "+callId)
            ctrl.Hold(callId)
            time.sleep(delay)
            print("UnHolding: "+callId)
            ctrl.UnHold(callId)


    def startCallTests(self, ctrl, localAccount, destAccount, nbIteration, delay, withHold):
            print("**[BEGIN] Call Test for account:"+localAccount);
            count = 0
            if(localAccount=='IP2IP'):
                    ctrl.setAccount(localAccount)

            count = 0
            while (count < nbIteration):
                print("["+str(count)+"/"+str(nbIteration)+"]");
                self.setRandomActiveCodecs(ctrl, localAccount)

                ctrl.Call(destAccount)
                callId=ctrl.getAllCalls()[0]

                if withHold:
                    delayHold = 5
                    nbHold = (delay) / (delayHold * 2)
                    countHold = 0

                    while (countHold < nbHold):
                        self.holdToggle(ctrl, callId, delayHold)
                        countHold = countHold + 1

                else:
                        time.sleep(delay)

                ctrl.HangUp(callId)
                count = count + 1

            print("**[END] Call Test for account:"+localAccount);

    def startSimultaneousCallTests(self, ctrl, localAccount, destAccount, nbIteration, delay, withHold, nbCalls):
            print("**[BEGIN] Call Test for account:"+localAccount);
            count = 0
            if(localAccount=='IP2IP'):
                    ctrl.setAccount(localAccount)

            count = 0
            while (count < nbIteration):
                print("["+str(count)+"/"+str(nbIteration)+"]");
                self.setRandomActiveCodecs(ctrl, localAccount)

                # do all the calls
                currCall = 0
                while (currCall < nbCalls):
                    ctrl.Call(destAccount)
                    time.sleep(1)
                    currCall = currCall +1

                #hold or wait for each call
                if withHold:
                    delayHold = 5
                    nbHold = (delay) / (delayHold * 2)
                    countHold = 0

                    while (countHold < nbHold):
                        for callId in ctrl.getAllCalls():
                            self.holdToggle(ctrl, callId, delayHold)
                        countHold = countHold + 1

                else:
                        time.sleep(delay)

                #hangup each call
                for callId in ctrl.getAllCalls():
                    ctrl.HangUp(callId)

                count = count + 1

            print("**[END] Call Test for account:"+localAccount);



    def start(self, ctrl):
        #testConfig
        if not (self.testConfig(ctrl)):
            print(("testConfig [KO]"))
            return;
        else:
            print(("testConfig [OK]"))



        #RING IP2IP tests
        self.startCallTests(ctrl,'IP2IP', RING_TEST_ACCOUNT, 1000,20, WITHOUT_HOLD)
        self.startCallTests(ctrl,'IP2IP', RING_TEST_ACCOUNT, 1000,20, WITH_HOLD)

        #Polycom IP2IP tests
        self.startCallTests(ctrl,'IP2IP', POLYCOM_TEST_ACCOUNT, 1000,20, WITHOUT_HOLD)
        self.startCallTests(ctrl,'IP2IP', POLYCOM_TEST_ACCOUNT, 1000,20, WITH_HOLD)

        #SIP tests
        sipAccount = ctrl.getAllAccounts('SIP')[0]
        self.registerAccount(ctrl, sipAccount)
        #self.startSimultaneousCallTests(ctrl,sipAccount, SIP_TEST_ACCOUNT, 10,40, WITHOUT_HOLD,10)
        self.startCallTests(ctrl,sipAccount, SIP_TEST_ACCOUNT, 1000,20, WITH_HOLD)
        self.startCallTests(ctrl,sipAccount, SIP_TEST_ACCOUNT, 1000,20, WITHOUT_HOLD)

        #DHT tests
        dhtAccount = ctrl.getAllAccounts('RING')[0]
        self.registerAccount(ctrl, dhtAccount)
        self.startCallTests(ctrl,dhtAccount, DHT_TEST_ACCOUNT, 10,60, WITHOUT_HOLD)
        self.startCallTests(ctrl,dhtAccount, DHT_TEST_ACCOUNT, 10,60, WITH_HOLD)
