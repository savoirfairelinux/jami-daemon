#! /usr/bin/env python3
#
# Copyright (C) 2004-2025 Savoir-faire Linux Inc.
# Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <https://www.gnu.org/licenses/>.

from controller import libjamiCtrl

import argparse
import sys
import signal
import os.path

class Controller(libjamiCtrl):
    # https://git.jami.net/savoirfairelinux/jami-daemon/-/blob/master/bin/dbus/cx.ring.Ring.ConfigurationManager.xml?ref_type=heads
    # dbus signature for "dataTransferEvent"
    # codeis described here:
    # https://git.jami.net/savoirfairelinux/jami-daemon/-/blob/master/src/jami/datatransfer_interface.h?ref_type=heads
    # in particular code==6 means "finished"
    def onDataTransferEvent(self, accountId, conversationId, interactionId, fileId, code):
        datatransfereventcode_finished=6
        if code != datatransfereventcode_finished:
            print("transfer %s has not been finished. Code returned is %s. Look at "
                "https://git.jami.net/savoirfairelinux/jami-daemon/-/blob/master/src/jami/datatransfer_interface.h?ref_type=heads"
                " to find out about the meaning of the code %s." % (interactionId, code, code))
            self.stopThread()
        else:
            print("Data transfer finished. onDataTransferEvent received: accountId %s, conversationId %s, "
                "interactionId %s, fileId (id+tid) %s, code %s" % (accountId, conversationId, interactionId, fileId, code))
            print("Stopping now.")
            self.stopThread()

parser = argparse.ArgumentParser(description="CLI to send a file to a jami (https://jami.net) conversation swarm. "
    "Example: python sendfile.py --account 387ccae41bb25e23 --conversation 5fe3d677bdd76a874e261df3759c500219fe839a --filename book.pdf")
parser.add_argument('--account', help='Account to use', metavar='<account>', type=str)
parser.add_argument('--conversation', help='Conversation identification related to used account', metavar='<conversation>', type=str, required=True)
parser.add_argument('--filename', help='Pathname on file to send', metavar='<filename>', type=str, required=True)
parser.add_argument('--displayname', help='Name displayed to peer', metavar='<displayname>', type=str)

args = parser.parse_args()
ctrl = Controller(sys.argv[0], False)

if not args.account:
    for account in ctrl.getAllEnabledAccounts():
        details = ctrl.getAccountDetails(account)
        if details['Account.type'] == 'RING':
            args.account = account
            print(f"{args.account} will be used as account")
            break
    if not args.account:
        print(f"No account found")
        raise ValueError("no valid account found")
if args.account not in ctrl.getAllEnabledAccounts():
    print(f"{args.account} is not a valid account")
    raise ValueError("not a valid account")
if args.conversation not in ctrl.listConversations(args.account):
    print(f"{args.conversation} is not a valid conversation")
    raise ValueError("not a valid conversation")
if not args.displayname:
    args.displayname = os.path.basename(args.filename)

tid = ctrl.sendFile(args.account, args.conversation, os.path.abspath(args.filename), args.displayname, "")

signal.signal(signal.SIGINT, ctrl.interruptHandler)

ctrl.run()
