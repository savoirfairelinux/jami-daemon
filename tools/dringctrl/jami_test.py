#! /usr/bin/env python3
#
#  Copyright (C) 2019 Savoir-faire Linux Inc. Inc
#
# Author: Mohamed Fenjiro <mohamed.fenjiro@savoirfairelinux.com>
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

import signal
import sys
import os
import time
import argparse

from gi.repository import GLib
from errorsDring import DRingCtrlError
from controller import DRingCtrl

class JamiTest(DRingCtrl):
    def __init__(self, name, args):
        super(JamiTest, self).__init__(name, False)
        self.args = args
        self.testCalls = set()
        self.iterator = 0
        self.failureCount = 0
        self.failureRate = 0
        self.callsCompleted = 0
        ringAccounts = self.getAllAccounts('RING')
        self.setAccount(ringAccounts[0])

        if not args.peer:
            peer = ringAccounts[1]
            details = self.getAccountDetails(peer)
            self.peer = details['Account.username']
        else:
            self.peer = args.peer

        print("Using local test account: ", self.account)
        print("Using test peer: ", self.peer)
        if self.testCall():
            GLib.timeout_add_seconds(args.interval, self.testCall)

    def keepGoing(self):
        return self.args.calls == 0 or self.iterator < self.args.calls

    def testCall(self):
        print("**[BEGIN] Call Test")
        if self.keepGoing():
            self.iterator += 1
            callId = self.Call(self.peer)
            self.testCalls.add(callId)
            GLib.timeout_add_seconds(self.args.duration, lambda: self.checkCall(callId))
        return self.keepGoing()

    def checkCall(self, callId):
        self.HangUp(callId)
        if callId in self.testCalls:
            self.testFailed(callId)
        return False

    def onCallStateChanged(self, callId, state, statecode):
        super().onCallStateChanged(callId, state, statecode)
        if callId in self.testCalls:
            if state == "RINGING":
                self.testSucceeded(callId)
                self.HangUp(callId)

    def testEnded(self, callId):
        self.testCalls.remove(callId)
        self.callsCompleted += 1
        self.failureRate = (self.failureCount / float(self.callsCompleted))
        print("Failure rate: ", self.failureRate)
        if not self.keepGoing():
            print("ENDING")
            self.stopThread()
            self.unregister()

    def testFailed(self, callId):
        self.failureCount += 1
        self.testEnded(callId)

    def testSucceeded(self, callId):
        self.testEnded(callId)
    
    def run(self):
        super().run()
        if self.failureCount == 0:
            sys.exit(0)
        elif self.failureRate < .5:
            sys.exit(1)
        else:
            sys.exit(2)

if __name__ == "__main__":

    parser = argparse.ArgumentParser(description='Monitor Jami reliabilty by mesuring failure rate for making Calls/Messages and receiving them.')
    parser.add_argument('--messages', help='Number of messages sent', type=int)
    parser.add_argument('--calls', help='Number of calls made', default=0, type=int)
    parser.add_argument('--duration', help='Specify the duration of the test (seconds)', default=10, type=int)
    parser.add_argument('--interval', help='Specify the test interval (seconds)', default=0, type=int)
    parser.add_argument('--peer', help='Specify the peer account id')

    args = parser.parse_args()

    test = JamiTest("test", args)
    test.run()