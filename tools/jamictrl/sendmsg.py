#! /usr/bin/env python3
#
# Copyright (C) 2004-2024 Savoir-faire Linux Inc.
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
    def onMessageSend(self, message):
        print("Daemon log message is %s", message)

parser = argparse.ArgumentParser(description="CLI to send a text message to a jami (https://jami.net) conversation swarm. "
    "Example: python sendmsg.py --account 387ccae41bb25e23 --conversation 5fe3d677bdd76a874e261df3759c500219fe839a --message 'Hello world'")
parser.add_argument('--account', help='Account to use', metavar='<account>', type=str)
parser.add_argument('--conversation', help='Conversation identification related to used account', metavar='<conversation>', type=str, required=True)
parser.add_argument('--message', help='Text to send', metavar='<message>', type=str, required=True)
parser.add_argument('--parent', help='Parent', metavar='<parent>', type=str)

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
if not args.parent:
    args.parent = ""

ctrl.sendMessage(args.account, args.conversation, args.message, args.parent)
