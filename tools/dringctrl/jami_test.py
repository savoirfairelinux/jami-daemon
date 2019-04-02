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
import os.path
from controller import DRingCtrl
import array as arr

from threading import Thread
from threading import Event
from errorsDring import DRingCtrlError

try:
    import dbus
    from dbus.mainloop.glib import DBusGMainLoop
except ImportError as e:
    raise DRingCtrlError("No python-dbus module found")


#Initialisation

class JamiTest(DRingCtrl):

    DBUS_DAEMON_OBJECT = 'cx.ring.Ring'
    DBUS_DAEMON_PATH = '/cx/ring/Ring'

    def __init__(self, name, autoAnswer, account, delay, peer, calls):
        super(JamiTest, self).__init__(name, autoAnswer)
        self.count = 0
        self.failureCounted = 0
        self.done = False
        #create one
        self.account = account
        self.array = arr.array('i', [])

    def registerAccount(self, account):
        if not account:
            print("registering:" + account)
            self.setAccountRegistered(account, True)

    def testCalls(self,failure):
        print("**[BEGIN] Call Test")
        i = 0

        while self.count < self.nbIteration:
            print("[%s/%s]" % (self.count, self.nbIteration))

            callId = self.Call(account, self.TestPeer) #verify same call before an after call onRinging

            if self.onCallStateChanged(self, callId, state, statecode):
                self.failureCounted += 1
                self.array[i] = self.failureCounted

            self.HangUp(callId)
            self.count += 1
            i += 1

        if self.failureCounted < failure:
            print("**[SUCCESS] Call Test")

        print("**[END] DHT Call Test")

    def acceptanceRate (self,statecode)
        if statecode == 1:
            
    
    def onCallStateChanged(self, callid, state, statecode):
        super().onCallStateChanged(callid, state, statecode)
        if state == "RINGING":
            self.acceptanceRate(1)
        elif state == "FAIL":
            self.acceptanceRate(0) 

#Main

if __name__ == "__main__":

    toolbar_width = 40
    sys.stdout.write("[%s]" % (" " * toolbar_width))
    sys.stdout.flush()
    sys.stdout.write("\b" * (toolbar_width+1))

    parser = argparse.ArgumentParser(description='Monitor Jami reliabilty by mesuring failure rate for making Calls/Messages and receiving them.')
    parser.add_argument('--messages', help='Number of messages sent', metavar='<messages>', type=int)
    parser.add_argument('--calls', help='Number of calls made', metavar='<calls>', type=int)
    parser.add_argument('--account', help='Specify which account to use', metavar='<account>', type=str)
    parser.add_argument('--peer', help='Peer identification related to used account', metavar='<peer>', type=str)
    parser.add_argument('--duration', help='Spcify the duration of the test', metavar='<duration>', type=int)
    parser.add_argument('--failure', help='Specify number MAX of failure accepted', metavar='<failure>', type=int)

    args = parser.parse_args()

    if not args.account:
        for account in ctrl.getAllEnabledAccounts():
            details = ctrl.getAccountDetails(account)
            if details['Account.type'] == 'RING':
                args.account = account
                break
        if not args.peer:
            raise ValueError("no valid account")

    if not args.peer:
        for account in ctrl.getAllEnabledAccounts():
            details = ctrl.getAccountDetails(account)
            if details['Account.type'] == 'RING' and account != args.account:
                args.peer = account
                break
        if not args.peer:
            raise ValueError("no valid peer")

    if not args.durarion:
        args.duration = 10

    if not args.failure:
        args.failure = 2

    if not args.calls:
        args.calls = 100

    if not args.messages:
        args.messages=100

    exemple = JamiTest(args.account, args.calls, args.failure,)
    for i in range(toolbar_width):
        time.sleep(0.1)
        exemple.testCalls(True, args.account, args.delay)

        sys.stdout.write("-")
        sys.stdout.flush()

    sys.stdout.write("\n")