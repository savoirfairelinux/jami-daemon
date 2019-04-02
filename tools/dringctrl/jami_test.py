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
import controller
import arr as array

from threading import Thread
from threading import Event
from errors import DRingCtrlError

try:
    import dbus
    from dbus.mainloop.glib import DBusGMainLoop
except ImportError as e:
    raise DRingCtrlError("No python-dbus module found")


#Initialisation

class JamiTest(Controller):

    toolbar_width = 40

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

    def test(self, nbIteration, delay, account):
        print("**[BEGIN] Call Test")
        i = 0

        while count < nbIteration:
            print("[%s/%s]" % (self.count, nbIteration))

            callId = self.Call(account, self.TestPeer)

            if self.onCallStateChanged(self.TestAccount) != "RINGING"
                failureCounted += 1
                self.array[i] = failureCounted

            sfl.HangUp(callId)
            count += 1
            i += 1
            time.sleep(delay)

        if failureCounted < failure
            print("**[SUCCESS] Call Test")

        print("**[END] DHT Call Test")

#Main

if __name__ == "__main__":

    sys.stdout.write("[%s]" % (" " * toolbar_width))
    sys.stdout.flush()
    sys.stdout.write("\b" * (toolbar_width+1))

    parser = argparse.ArgumentParser(description='Monitor Jami reliabilty by mesuring failure rate for making Calls/Messages and receiving them.')
    parser.add_argument('--messages', help='Number of messages sent', metavar='<messages>', type=int)
    parser.add_argument('--calls', help='Number of calls made', metavar='<calls>', type=int)
    parser.add_argument('--level', help='Max number of failure accepted', metavar='<failure>', type=int)
    parser.add_argument('--account', help='Specify which account to use', metavar='<account>', type=str)
    parser.add_argument('--peer', help='Peer identification related to used account', metavar='<peer>', type=str)
    parser.add_argument('--duration', help='Spcify the duration of the test', metavar='<duration>', type=int)
    parser.add_argument('--failure', help='Specify number MAX of failure accepted', metavar='<failure>', type=int)

    args = parser.parse_args()

    if not args.peer:
        for account in ctrl.getAllEnabledAccounts():
            details = ctrl.getAccountDetails(account)
            if details['Account.type'] == 'RING' and account != args.account:
                args.peer = account
                break
        if not args.peer:
            raise ValueError("no valid peer")

    if not args.peer:
        for account in ctrl.getAllEnabledAccounts():
            details = ctrl.getAccountDetails(account)
            if details['Account.type'] == 'RING' and account != args.account:
                args.peer = account
                break
        if not args.peer:
            raise ValueError("no valid peer")

    if not args.frequency:
        args.frequency = 1
        frequency = args.frequency

    if not args.durarion:
        args.duration = 10

    exemple = JamiTest("efjeo", True)
    for i in range(toolbar_width):
        time.sleep(0.1)
        exemple.test(True, args.account, args.delay)
        exemple.set()

        sys.stdout.write("-")
        sys.stdout.flush()

    sys.stdout.write("\n")

