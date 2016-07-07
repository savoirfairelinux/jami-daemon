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

from flask import Flask
from flask_restful import Api

import threading
import asyncio
import websockets
from websockets import exceptions as ws_ex

from ring_api.server.flask.cb_api import websockets as cb_api
from ring_api.server.flask.api import account, video, calls, certificate, audio, crypto, codec

class FlaskServer:

    websockets = list()
    ws_messages = asyncio.Queue()

    def __init__(self, host, port, dring):
        self.host = host
        self.port = port
        self.dring = dring

        self.app = Flask(__name__)
        self.app.config['SECRET_KEY'] = 't0p_s3cr3t'
        self.app.config.update(
            PROPAGATE_EXCEPTIONS = True
        )
        self.api = Api(self.app, catch_all_404s=True)

        self._add_resources()

    def _add_resources(self):
        """Keep the same order as in the rest-api.json."""

        # Accounts

        self.api.add_resource(account.Account, '/account/',
            resource_class_kwargs={'dring': self.dring})

        self.api.add_resource(account.Accounts, '/accounts/',
            resource_class_kwargs={'dring': self.dring})

        self.api.add_resource(account.AccountsID, '/accounts/<account_id>/',
            resource_class_kwargs={'dring': self.dring})

        self.api.add_resource(account.AccountsDetails,
            '/accounts/<account_id>/details/',
            resource_class_kwargs={'dring': self.dring})

        self.api.add_resource(account.AccountsCodecs,
            '/accounts/<account_id>/codecs/',
            '/accounts/<account_id>/codecs/<codec_id>/',
            resource_class_kwargs={'dring': self.dring})

        self.api.add_resource(account.AccountsCall,
            '/accounts/<account_id>/call/',
            resource_class_kwargs={'dring': self.dring})

        self.api.add_resource(account.AccountsCertificates,
            '/accounts/<account_id>/certificates/<cert_id>/',
            resource_class_kwargs={'dring': self.dring})

        # Calls

        self.api.add_resource(calls.Calls,
            '/calls/<call_id>/',
            resource_class_kwargs={'dring': self.dring})

        # Codecs

        self.api.add_resource(codec.Codecs,
            '/codecs/',
            resource_class_kwargs={'dring': self.dring})

        # Crypto

        self.api.add_resource(crypto.Tls,
            '/crypto/tls/',
            resource_class_kwargs={'dring': self.dring})


        # Certificate

        self.api.add_resource(certificate.Certificate,
            '/certificates/',
            resource_class_kwargs={'dring': self.dring})

        self.api.add_resource(certificate.Certificates,
            '/certificate/<cert_id>/',
            resource_class_kwargs={'dring': self.dring})

        # Audio

        self.api.add_resource(audio.Plugins,
            '/audio/plugins/',
            resource_class_kwargs={'dring': self.dring})

        # Video

        self.api.add_resource(video.VideoDevices,
            '/video/devices/',
            resource_class_kwargs={'dring': self.dring})

        self.api.add_resource(video.VideoSettings,
            '/video/<device_id>/settings/',
            resource_class_kwargs={'dring': self.dring})

        self.api.add_resource(video.VideoCamera,
            '/video/camera/',
            resource_class_kwargs={'dring': self.dring})

    def register_callbacks(self, class_instance):
        """ dring callbacks register using this class instance """
        callbacks = self.dring.callbacks_to_register()

        # TODO add dynamically from implemented function names
        callbacks['text_message'] = cb_api.text_message

        self.dring.register_callbacks(callbacks, context=class_instance)

    def start_ws_eventloop(self):
        """ thread started from client """
        self.ws_eventloop = asyncio.new_event_loop()
        asyncio.set_event_loop(self.ws_eventloop)

        # TODO register
        self.ws_server = websockets.serve(
                self.ws_handle, '127.0.0.1', 5678)

        self.ws_eventloop.create_task(self.ws_server)
        self.ws_eventloop.create_task(self.ws_notify())

        self.ws_eventloop.run_forever()

    def start_rest(self):
        """ thread started from client

        use_reloader -- False because expects to run in the main thread
        """
        self.app.run(host=self.host, port=self.port,
                debug=True, use_reloader=False)

    def stop(self):
        # TODO
        pass

    # WebSockets using AsyncIO

    async def ws_handle(self, websocket, path):
        """ task handle which is run for every websocket """
        if (websocket not in self.websockets):
            self.websockets.append(websocket)
            print('server: adding new socket: %s' % str(websocket))

        print('server: sending "welcome" to %s' % str(websocket))
        await websocket.send('welcome')

        while True:
            # keeps the websocket alive by the current design
            # see: https://github.com/aaugustin/websockets/issues/122
            await asyncio.sleep(60)
            if (websocket not in self.websockets):
                print('server: closing websocket %s' % websocket)
                break

    async def ws_notify(self):
        """ task notify which listens for new callback messages """
        print('server: waiting for websockets notifications')
        while True:
            # FIXME messages are added but not retrieved
            message = await self.ws_messages.get()
            print('server: got "%s"' % message)

            for websocket in self.websockets:
                print('server: sending "%s" to %s' % (message, websocket))
                try:
                    await websocket.send(message)
                except ws_ex.ConnectionClosed:
                    self.websockets.remove(websocket)
                    print('server: connection closed to %s' % websocket)

    def callback_to_ws(self, message):
        """ used from the python callbacks in the dring """
        print('server: adding "%s" to ws_messages queue of size %s' % (
            message, self.ws_messages.qsize(),))
        self.ws_eventloop.call_soon_threadsafe(
            self.ws_messages.put_nowait, message)

