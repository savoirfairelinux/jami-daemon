#! /usr/bin/env python3
#
# Copyright (C) 2004-2023 Savoir-faire Linux Inc.
# Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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

from controller import libjamiCtrl

import argparse
import sys
import signal
import os.path

class Controller(libjamiCtrl):
    def onDataTransferEvent(self, transferId, code):
        if code == 6:
            print("transfer %u has been cancelled by host" % transferId)
            self.stopThread()
        else:
            print("TID %x, code %u" % (transferId, code))

parser = argparse.ArgumentParser()
parser.add_argument('--account', help='Account to use', metavar='<account>', type=str)
parser.add_argument('--peer', help='Peer identification related to used account', metavar='<peer>', type=str, required=True)
parser.add_argument('--filename', help='Pathname on file to send', metavar='<filename>', type=str, required=True)
parser.add_argument('--displayname', help='Name displayed to peer', metavar='<displayname>', type=str)

args = parser.parse_args()
ctrl = Controller(sys.argv[0], False)

if not args.account:
    for account in ctrl.getAllEnabledAccounts():
        details = ctrl.getAccountDetails(account)
        if details['Account.type'] == 'RING':
            args.account = account
            break
    if not args.account:
        raise ValueError("no valid account")
if not args.displayname:
    args.displayname = os.path.basename(args.filename)

tid = ctrl.sendFile(args.account, args.peer, os.path.abspath(args.filename), args.displayname)

signal.signal(signal.SIGINT, ctrl.interruptHandler)

ctrl.run()
