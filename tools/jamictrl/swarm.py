#!/usr/bin/env python
#
#  Copyright (C) 2020-2023 Savoir-faire Linux Inc.
#
# Author: Sébastien Blin <sebastien.blin@savoirfairelinux.com>
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

from controller import libjamiCtrl

import argparse
import sys
import signal
import os.path
import threading

parser = argparse.ArgumentParser()
parser.add_argument('--account', help='Account to use', metavar='<account>', type=str)

args = parser.parse_args()

ctrl = libjamiCtrl(sys.argv[0], False)
if not args.account:
    for account in ctrl.getAllEnabledAccounts():
        details = ctrl.getAccountDetails(account)
        if details['Account.type'] == 'RING':
            args.account = account
            break
    if not args.account:
        raise ValueError("no valid account")

def run_controller():
    ctrl.run()

if __name__ == "__main__":
    ctrlThread = threading.Thread(target=run_controller, args=(), daemon=True)
    ctrlThread.start()
    while True:
        print("""Swarm options:
0. Create conversation
1. List conversations
2. List conversations members
3. Add Member
4. List requests
5. Accept request
6. Decline request
7. Load messages
8. Send message
9. Remove conversation
        """)
        opt = int(input("> "))
        if opt == 0:
            ctrl.startConversation(args.account)
        elif opt == 1:
            print(f'Conversations for account {args.account}:')
            for conversation in ctrl.listConversations(args.account):
                print(f'\t{conversation}')
        elif opt == 2:
            conversationId = input('Conversation: ')
            print(f'Members for conversation {conversationId}:')
            for member in ctrl.listConversationsMembers(args.account, conversationId):
                print(f'{member["uri"]}')
        elif opt == 3:
            conversationId = input('Conversation: ')
            contactUri = input('New member: ')
            ctrl.addConversationMember(args.account, conversationId, contactUri)
        elif opt == 4:
            print(f'Conversations request for account {args.account}:')
            for request in ctrl.listConversationsRequests(args.account):
                print(f'{request["id"]}')
        elif opt == 5:
            conversationId = input('Conversation: ')
            ctrl.acceptConversationRequest(args.account, conversationId)
        elif opt == 6:
            conversationId = input('Conversation: ')
            ctrl.declineConversationRequest(args.account, conversationId)
        elif opt == 8:
            conversationId = input('Conversation: ')
            message = input('Message: ')
            ctrl.sendMessage(args.account, conversationId, message)
        elif opt == 9:
            conversationId = input('Conversation: ')
            ctrl.removeConversation(args.account, conversationId)
        else:
            print('Not implemented yet')
    ctrlThread.join()
