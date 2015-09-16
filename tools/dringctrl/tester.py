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




WITH_HOLD = True
WITHOUT_HOLD = False

ALL_TEST_NAME = {
        'TestConfig': 'testConfig',
        'LoopCallDht': 'testLoopCallDht',
        'VideoBirtateDHT': 'testLoopCallDhtWithIncBitrate',
        'SimultenousDHTCall' : 'testSimultaneousLoopCallDht',
        'DhtCallHold' : 'testLoopCallDhtWithHold'
        }

class DRingTester():

    DHT_ACCOUNT_ID = ''
    SIP_ACCOUNT_ID = ''
    # Dht Client
    SIP_TEST_ACCOUNT = 'sf1'
    # Dht Client
    DHT_TEST_ACCOUNT = '280ca11317ec90a939c86fbfa06532dbb8a08f8a'
    # Ring Client
    RING_TEST_ACCOUNT = '192.168.50.143'
    # Polycom Client
    POLYCOM_TEST_ACCOUNT = '192.168.40.38'
    FILE_TEST = '/home/eloi/Videos/Mad_Max_Fury_Road_2015_Trailer_F4_5.1-1080p-HDTN.mp4'

    minBitrate = 400
    maxBitrate = 4000
    incBitrate = 100

    def testConfig(self, ctrl):
        print("**[BEGIN] test config")
        allCodecs = ctrl.getAllCodecs()
        if len(allCodecs) == 0:
            print("error no codec on the system")
            return 0

        print("**[SUCCESS] test config")
        print("**[END] test config")
        return 1
#
# Helpers
#
    def getTestName(self):
        return ALL_TEST_NAME.keys()

    def checkIP2IPAccount(self, ctrl):
        ipAccount = ctrl.getAllAccounts()
        print(ipAccount)
        if len(ipAccount) == 0:
            print("no IP2IP account")
            return 0
        return 1

    def registerAccount(self, ctrl, account):
        print("registering:" + account)
        ctrl.setAccountRegistered(account, True)

    def setRandomActiveCodecs(self, ctrl, account):
        codecList = ctrl.getAllCodecs()
        shuffle(codecList)
        ctrl.setActiveCodecList(account, str(codecList)[1:-1])
        print("New active list for " + account)
        print(ctrl.getActiveCodecs(account))

    def holdToggle(self, ctrl, callId, delay):
        time.sleep(delay)
        print("Holding: " + callId)
        ctrl.Hold(callId)
        time.sleep(delay)
        print("UnHolding: " + callId)
        ctrl.UnHold(callId)

#
# tests
#

# testLoopCallDht
# perform <nbIteration> DHT calls using <delay> between each call

    def testLoopCallDht(self, ctrl, nbIteration, delay):
        print("**[BEGIN] DHT Call Test")

        count = 0
        while count < nbIteration:
            print("[%s/%s]" % (count, nbIteration))

            self.setRandomActiveCodecs(ctrl, self.DHT_ACCOUNT_ID)

            callId = ctrl.Call(self.DHT_TEST_ACCOUNT)

            # switch to file input
            ctrl.switchInput(callId,'file://'+self.FILE_TEST)

            time.sleep(delay)

            ctrl.HangUp(callId)
            count += 1

        print("**[END] DHT Call Test")

# testLoopCallDhtWithHold
# perform <nbIteration> DHT calls using <delay> between each call
# perform stress hold/unhold between each call

    def testLoopCallDhtWithHold(self, ctrl, nbIteration, delay):
        print("**[BEGIN] DHT Call Test")

        count = 0
        while count < nbIteration:
            print("[%s/%s]" % (count, nbIteration))

            self.setRandomActiveCodecs(ctrl, self.DHT_ACCOUNT_ID)

            callId = ctrl.Call(self.DHT_TEST_ACCOUNT)

            # switch to file input
            ctrl.switchInput(callId,'file://'+self.FILE_TEST)

            delayHold = 5
            nbHold = delay / (delayHold * 2)
            countHold = 0

            while countHold < nbHold:
                self.holdToggle(ctrl, callId, delayHold)
                countHold = countHold + 1


            ctrl.HangUp(callId)
            count += 1

        print("**[END] DHT Call Test")


# testLoopCallDhtWithIncBitrate
# perform <nbIteration> DHT calls using <delay> between each call
# inc bitrate between each iteration

    def testLoopCallDhtWithIncBitrate(self, ctrl, nbIteration, delay):
        print("**[BEGIN] VIDEO Bitrate Test")

        count = 0
        currBitrate = self.minBitrate

        while count < nbIteration:
            print("[%s/%s]" % (count, nbIteration))

            self.setRandomActiveCodecs(ctrl, self.DHT_ACCOUNT_ID)
            print("setting video bitrate to "+str(currBitrate))
            ctrl.setCodecBitrate(self.DHT_ACCOUNT_ID, currBitrate)

            callId = ctrl.Call(self.DHT_TEST_ACCOUNT)

            # switch to file input
            ctrl.switchInput(callId,'file://'+self.FILE_TEST)

            time.sleep(delay)

            ctrl.HangUp(callId)
            count += 1

            currBitrate += self.incBitrate
            if (currBitrate > self.maxBitrate):
                currBitrate = self.minBitrate

        print("**[END] VIDEO Bitrate Test")

# testSimultaneousLoopCallDht
# perform <nbIteration> simultaneous DHT calls using <delay> between each call

    def testSimultaneousLoopCallDht(self, ctrl, nbIteration, delay):
        count = 0
        while count < nbIteration:
            print("[%s/%s]" % (count, nbIteration))
            self.setRandomActiveCodecs(ctrl, self.DHT_ACCOUNT_ID)

            # do all the calls
            currCall = 0
            NB_SIMULTANEOUS_CALL = 10;
            while currCall <= NB_SIMULTANEOUS_CALL:
                ctrl.Call(self.DHT_TEST_ACCOUNT)
                time.sleep(1)
                currCall = currCall + 1

                time.sleep(delay)

            # hangup each call
            for callId in ctrl.getAllCalls():
                ctrl.HangUp(callId)

            count += 1

        print("**[END] Call Test for account:" + localAccount)




# Main function

    def start(self, ctrl, testName):

        if not testName in ALL_TEST_NAME:
            print(("wrong test name"))
            return

        #getConfig
        self.DHT_ACCOUNT_ID = ctrl.getAllAccounts('RING')[0]
        self.SIP_ACCOUNT_ID = ctrl.getAllAccounts('SIP')[0]
        TEST_NB_ITERATION = 100
        TEST_DELAY = 30

        getattr(self, ALL_TEST_NAME[testName])(ctrl,TEST_NB_ITERATION, TEST_DELAY)



"""

        # DHT tests
        dhtAccount = ctrl.getAllAccounts('RING')[0]
        self.startDynamicBitrateCallTests(ctrl, dhtAccount, DHT_TEST_ACCOUNT, 100, 60, 250, 4000, 100)
        self.startCallTests(ctrl, dhtAccount, DHT_TEST_ACCOUNT, 100, 60, WITHOUT_HOLD)
        self.startCallTests(ctrl, dhtAccount, DHT_TEST_ACCOUNT, 100, 60, WITH_HOLD)

        # RING IP2IP tests
        self.startCallTests(ctrl, 'IP2IP', RING_TEST_ACCOUNT, 1000, 20, WITHOUT_HOLD)
        self.startCallTests(ctrl, 'IP2IP', RING_TEST_ACCOUNT, 1000, 20, WITH_HOLD)



        # Polycom IP2IP tests
        self.startCallTests(ctrl, 'IP2IP', POLYCOM_TEST_ACCOUNT, 1000, 20, WITHOUT_HOLD)
        self.startCallTests(ctrl, 'IP2IP', POLYCOM_TEST_ACCOUNT, 1000, 20, WITH_HOLD)

        # SIP tests
        sipAccount = ctrl.getAllAccounts('SIP')[0]
        # self.startSimultaneousCallTests(ctrl, sipAccount, SIP_TEST_ACCOUNT, 10, 40, WITHOUT_HOLD,10)
        self.startCallTests(ctrl, sipAccount, SIP_TEST_ACCOUNT, 1000, 20, WITH_HOLD)
        self.startCallTests(ctrl, sipAccount, SIP_TEST_ACCOUNT, 1000, 20, WITHOUT_HOLD)
"""
