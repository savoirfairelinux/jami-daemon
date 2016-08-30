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
import argparse, time
from queue import Queue
from threading import Thread

# generated shared library
from ring_api.dring_cython import Dring

from ring_api.server.flask.server import FlaskServer

def options_parser(desc=None, force_rest=False):
    """ Return the parser without parsing the args """

    if (not desc):
        desc='Python bindings on the Ring-daemon library'

    parser = argparse.ArgumentParser(description=desc)

    parser.add_argument('-v', '--verbose',
        action='store_true', dest='verbose', default=False,
        help='activate all of the verbose options')

    parser.add_argument('-d', '--debug',
        action='store_true', dest='debug', default=False,
        help='debug mode (more verbose)')

    parser.add_argument('-c', '--console',
        action='store_true', dest='console', default=False,
        help='log in console (instead of syslog)')

    parser.add_argument('-p', '--persistent',
        action='store_true', dest='persistent', default=False,
        help='stay alive after client quits')

    rest_default = False
    rest_help = 'start with a restful server'

    if (force_rest):
        rest_default = True
        rest_help = argparse.SUPPRESS

    parser.add_argument('-r', '--rest',
        action='store_true', dest='rest', default=rest_default,
        help=rest_help)

    parser.add_argument('--host',
        dest='host', default='127.0.0.1',
        help='restful server host')

    parser.add_argument('--port',
        type=int, dest='http_port', default=8080,
        help='server http port for rest')

    parser.add_argument('--ws-port',
        type=int, dest='ws_port', default=5678,
        help='server websocket port for callbacks')

    parser.add_argument('--auto-answer',
        action='store_true', dest='autoanswer', default=False,
        help='force automatic answer to incoming call')

    parser.add_argument('--dring-version',
        action='store_true', dest='dring_version', default=False,
        help='show Ring-daemon version')

    interpreter_help='adapt threads for interpreter interaction'

    if (force_rest):
        interpreter_help = argparse.SUPPRESS

    parser.add_argument('--interpreter',
        action='store_true', dest='interpreter', default=False,
        help=interpreter_help)

    return parser

def options():
    """ Parse the options with args (used by a user script)
        and return the result (used by interpreter to define them dynamically).
    """
    parser = options_parser()
    return parser.parse_args()

class Client:

    def __init__(self, _options=None):
        self.dring = Dring()
        self.dring_pollevents_interval = 0.1

        if (not _options):
            _options = options()
        self.options = _options

        if (self.options.verbose):
            self.options.debug = True
            self.options.console = True

        bitflags = self.options_to_bitflags(self.options)
        self.__init_threads__(bitflags)

        if (self.options.dring_version):
            print(self.dring.version())

    def __init_threads__(self, bitflags):
        # 1. Ring-daemon (dring) thread
        self.dring.init_library(bitflags)
        self.thread_dring = Thread(target=self.dring.start)
        self.thread_dring.setDaemon(not self.options.persistent)

        if (self.options.rest):
            # Initialize the server with dring instance
            self.server = FlaskServer(
                    self.options.host, self.options.http_port,
                    self.options.ws_port, self.dring_pollevents_interval,
                    self.dring, self.options.verbose)

            # 2. Dring pollevents thread
            self.thread_dring_pollevents = Thread(
                target=self._run_dring_pollevents)

            # 3. Server rest app thread
            self.thread_server_restapp = Thread(target=self.server.run_rest)

        if (self.options.interpreter):
            # Main loop as daemon for non blocking interpreter mode
            self.thread_mother = Thread(target=self._run_main_loop)
            self.thread_mother.setDaemon(True)

    def _run_dring_pollevents(self):
        """ Runs Ring-daemon (dring) poll_events() on an interval """
        while True:
            time.sleep(self.dring_pollevents_interval)
            self.dring.poll_events()

    def _run_main_loop(self):
        # 1. Ring-daemon (dring) thread for everyone
        self.thread_dring.start()

        if (self.options.rest):
            # give dring time to init
            time.sleep(3)

            # 2. dring pollevents thread
            self.thread_dring_pollevents.start()

            # 3. restapp thread
            self.thread_server_restapp.start()

            # main process needs to be websockets asyncio eventloop
            self.server.run_websockets()

        else:
            self._run_dring_pollevents()

    def start(self):
        """ Starts the threading according to the users options.

        In interpreter mode, the main_loop is self-contained in a thread to
        enable a non-blocking runtime.
        """
        if (self.options.interpreter and self.options.rest):
            raise RuntimeError("REST Server wasn't designed to run in interpreter.")

        try:
            if (self.options.interpreter):
                self.thread_mother.start()
            else:
                self._run_main_loop()

        except (KeyboardInterrupt, SystemExit):
            self.stop()

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
