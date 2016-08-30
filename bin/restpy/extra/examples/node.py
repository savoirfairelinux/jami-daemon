#!/usr/bin/env python3
#
# Copyright (C) 2016 Savoir-faire Linux Inc
#
# Author: Seva Ivanov <seva.ivanov@savoirfairelinux.com>
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

import argparse, json, time
from queue import Queue
from threading import Thread
from datetime import datetime

from ring_api import client as ring_api

LOG_RECEIVED = ("\033[1;32m[Received    ] \033[1;00'%s'" +
    " : rerouting all messages to '%s'")
LOG_FORWARD = "\033[1;32m[Forwarding  ] \033[1;00m'%s'"

class EchoBot:

    messages = Queue()
    requests = Queue()  # clients aliases

    def __init__(self, ring_api, clients, verbose):
        self.ring_api = ring_api
        self.clients = clients
        self.verbose = verbose

        self.forward_text_thread = Thread(target=self.forward_text)
        self.forward_text_thread.start()

    def set_account(self, account):
        self.account = account

    def forward_text(self):

        while True:

            alias = self.requests.get()
            to_ring_id = self.clients[alias]

            while not self.messages.empty():

                message = self.messages.get()
                log = str.join(' : ', ('[' + str(message['datetime']),
                                       message['from_ring_id'] + ']'))

                if (not message['content']['text/plain']):
                    message['content']['text/plain'] = log
                else:
                    message['content']['text/plain'] = str.join(
                        ' : ', (log, message['content']['text/plain']))

                if (self.verbose):
                    print(LOG_FORWARD % message['content']['text/plain'])

                self.ring_api.dring.config.send_account_message(
                    self.account, to_ring_id, message['content'])

                # Prevents blocking Ring-daemon
                time.sleep(1)

    def on_text(self, account_id, from_ring_id, content):

        command = None

        if (content['text/plain']):

            text_message = content['text/plain']

            for alias in self.clients:

                if (text_message == '!echo %s reply' % alias):

                    command = text_message

                    if (self.verbose):
                        # FIXME: bad bang '!' parsing resulting in loosing '!e'
                        print(LOG_RECEIVED % (command, self.clients[alias]))

                    self.requests.put(alias)

        if (not command):
            self.messages.put({
                'datetime': datetime.now(),
                'account_id': account_id,
                'from_ring_id': from_ring_id,
                'content': content
            })

def options_parser():

    parser = argparse.ArgumentParser(
        description='Ring API node using bots')

    parser.add_argument('-v', '--verbose', action='store_true')

    parser.add_argument('-c', '--clients', type=json.loads, required=True,
            help='Clients as JSON string of %s{"alias": "ring_id"}%s' %
                ("'", "'",))

    return parser

if __name__ == "__main__":

    parser = options_parser()
    opts = parser.parse_args()

    ring_args = ['-v'] if opts.verbose else []
    ring_parser = ring_api.options_parser()
    ring_opts = ring_parser.parse_args(ring_args)

    ring_opts.verbose = opts.verbose
    ring_opts.interpreter = True
    ring_api = ring_api.Client(ring_opts)

    echo_bot = EchoBot(ring_api, opts.clients, opts.verbose)

    cbs = ring_api.dring.callbacks_to_register()
    cbs['account_message'] = echo_bot.on_text
    ring_api.dring.register_callbacks(cbs)

    ring_api.start()

    time.sleep(1)

    accounts = ring_api.dring.config.accounts()
    echo_bot.set_account(accounts[0])

    print('\033[1;32m[Ring node is listening]')

    try:
        while True:
            time.sleep(1)

    except (KeyboardInterrupt, SystemExit):
        ring_api.stop()
