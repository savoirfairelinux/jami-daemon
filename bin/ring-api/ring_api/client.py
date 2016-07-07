#
# Copyright (C) 2016 Savoir-faire Linux Inc
#
# Authors:  Seva Ivanov <seva.ivanov@savoirfairelinux.com>
#           Simon Zeni  <simon.zeni@savoirfairelinux.com>
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

from optparse import OptionParser
from bottle import Bottle

import threading, time
from queue import Queue

from ring_api.dring_cython import Dring
from ring_api.server.flask.server import FlaskServer

def options():
    usage = 'usage: %prog [options] arg1 arg2'
    parser = OptionParser(usage=usage)

    parser.add_option('-v', '--verbose',
        action='store_true', dest='verbose', default=False,
        help='activate all of the verbose options')

    parser.add_option('-d', '--debug',
        action='store_true', dest='debug', default=False,
        help='debug mode (more verbose)')

    parser.add_option('-c', '--console',
        action='store_true', dest='console', default=False,
        help='log in console (instead of syslog)')

    parser.add_option('-p', '--persistent',
        action='store_true', dest='persistent', default=False,
        help='stay alive after client quits')

    parser.add_option('-r', '--rest',
        action='store_true', dest='rest', default=False,
        help='start with restful server api')

    parser.add_option('--port',
        type='int', dest='port', default=8080,
        help='restful server port')

    parser.add_option('--host',
        type='str', dest='host', default='127.0.0.1',
        help='restful server host')

    parser.add_option('--auto-answer',
        action='store_true', dest='autoanswer', default=False,
        help='force automatic answer to incoming call')

    parser.add_option('--dring-version',
        action='store_true', dest='dring_version', default=False,
        help='show Ring-daemon version')

    parser.add_option('--interpreter',
        action='store_true', dest='interpreter', default=False,
        help='adapt threads for interpreter interaction')

    return parser.parse_args()

class Client:

    def __init__(self, _options=None):
        self.dring = Dring()

        if (not _options):
            (_options, args) = options()
        self.options = _options

        if (self.options.verbose):
            self.options.debug = True
            self.options.console = True

        bitflags = self.options_to_bitflags(self.options)
        self.__init_threads__(bitflags)

        if (self.options.dring_version):
            print(self.dring.version())

    def __init_threads__(self, bitflags):
        self.dring.init_library(bitflags)
        self.dring_thread = threading.Thread(target=self.dring.start)
        self.dring_thread.setDaemon(not self.options.persistent)

        if (self.options.rest):
            # create the rest server
            self.restapp = FlaskServer(
                self.options.host, self.options.port, self.dring)

            # init websockets asyncio eventloop thread
            self.restapp_ws_eventloop_thread = threading.Thread(
                target=self.restapp.start_ws_eventloop)

            # init restapp thread
            self.restapp_thread = threading.Thread(
                target=self.restapp.start_rest)

        if (self.options.interpreter):
            # main loop as daemon (foreground process)
            self.mother_thread = threading.Thread(
                    target=self._start_main_loop)
            self.mother_thread.setDaemon(True)

    def start(self):
        try:
            if (self.options.interpreter):
                self.mother_thread.start()
            else:
                self._start_main_loop()

        except (KeyboardInterrupt, SystemExit):
            self.stop()

    def _start_main_loop(self):
        self.dring_thread.start()

        if (self.options.rest):
            # give dring time to init
            time.sleep(3)

            # start websockets asyncio eventloop in a thread
            self.restapp_ws_eventloop_thread.start()

            # register callbacks which will use the server instace
            self.restapp.register_callbacks(self.restapp)

            # start restapp in a thread
            self.restapp_thread.start()

        while True:
            time.sleep(0.1)
            self.dring.poll_events()

    def stop(self):
        if (self.options.verbose):
            print("Finishing..")

        self.dring.stop()

        if hasattr(self, 'restapp'):
            self.restapp.stop()

    def options_to_bitflags(self, options):
        flags = 0

        if (options.console):
            flags |= self.dring._FLAG_CONSOLE_LOG

        if (options.debug):
            flags |= self.dring._FLAG_DEBUG

        if (options.autoanswer):
            flags |= self.dring._FLAG_AUTOANSWER

        return flags

