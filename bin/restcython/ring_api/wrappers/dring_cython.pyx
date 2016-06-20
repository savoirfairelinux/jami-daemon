# cython: language_level=3
#
# Copyright (C) 2016 Savoir-faire Linux Inc
#
# Author: Seva Ivanov <seva.ivanov@savoirfairelinux.com>
# Author: Simon Zeni <simon.zeni@savoirfairelinux.com>
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

# Keep the logic of the interfaces
# Apply the logical partitioning in the API layer

from libcpp.string cimport string
from libcpp.map cimport map as map

from ring_api.utils.std cimport *
from ring_api.interfaces cimport dring as dring_cpp
from ring_api.interfaces cimport configuration_manager as confman_cpp
from ring_api.interfaces cimport video_manager as videoman_cpp
from ring_api.interfaces cimport cb_client as cb_client_cpp

global python_callbacks
python_callbacks = dict.fromkeys(['text_message'])

cdef public void incoming_account_message(
        const string& raw_account_id,
        const string& raw_from_ring_id,
        const map[string, string]& raw_content):

    account_id = bytes(raw_account_id).decode()
    from_ring_id = bytes(raw_from_ring_id).decode()

    content = dict()
    raw_content_dict = dict(raw_content)
    for raw_key in raw_content_dict:
        key = raw_key.decode()
        content[key] = raw_content_dict[raw_key].decode()

    global python_callbacks
    callback = python_callbacks['text_message']
    if (callback):
        callback(str(account_id), str(from_ring_id), content)

cdef class CallbacksClient:
    cdef cb_client_cpp.CallbacksClient *_thisptr

    def __cinit__(self):
        self._thisptr = new cb_client_cpp.CallbacksClient()

    def __dealloc__(self):
        del self._thisptr

    def register_events(self):
        """Registers cython callback events"""

        self._thisptr.registerEvents()

cdef class ConfigurationManager:

    def accounts(self):
        """List user accounts (not ring ids)

        Return: accounts list
        """
        accounts = list()
        raw_accounts = confman_cpp.getAccountList()

        for i, account in enumerate(raw_accounts):
            accounts.append(account.decode())

        return accounts

    def account_details(self, account_id):
        """Gets account details

        Keyword arguments:
        account_id -- account id string

        Return: account details dict
        """
        cdef string raw_id = account_id.encode()
        details = dict()
        raw_dict = confman_cpp.getAccountDetails(raw_id)

        for key, value in raw_dict.iteritems():
            details[key.decode()] = value.decode()

        return details

    def send_text_message(self, account_id, ring_id, content):
        """Sends a text message

        Keyword arguments:
        account_id  -- account id string
        ring_id     -- ring id destination string
        content     -- dict of content defined as {<mime-type>: <message>}

        No return
        """
        cdef string raw_account_id = account_id.encode()
        cdef string raw_ring_id = ring_id.encode()
        cdef map[string, string] raw_content

        for key, value in content.iteritems():
            raw_content[key.encode()] = value.encode()

        confman_cpp.sendAccountTextMessage(
                raw_account_id, raw_ring_id, raw_content)

cdef class VideoManager:
    def devices(self):
        """List the available video devices

        Return: devices list
        """
        devices = list()
        raw_devices = videoman_cpp.getDeviceList()

        for device in raw_devices:
            devices.append(device.decode())

        return devices

cdef class Dring:
    cdef:
        readonly int _FLAG_DEBUG
        readonly int _FLAG_CONSOLE_LOG
        readonly int _FLAG_AUTOANSWER

    cdef public ConfigurationManager config
    cdef public VideoManager video
    cdef CallbacksClient cb_client

    def __cinit__(self):
        self._FLAG_DEBUG          = dring_cpp.DRING_FLAG_DEBUG
        self._FLAG_CONSOLE_LOG    = dring_cpp.DRING_FLAG_CONSOLE_LOG
        self._FLAG_AUTOANSWER     = dring_cpp.DRING_FLAG_AUTOANSWER

        self.config = ConfigurationManager()
        if (not self.config):
            raise RuntimeError

        self.video = VideoManager()
        if (not self.video):
            raise RuntimeError

    def init_library(self, bitflags=0):

        if (not dring_cpp.init(bitflags)):
            raise RuntimeError

        self.cb_client = CallbacksClient()
        self.cb_client.register_events()

    def start(self):
        if (not dring_cpp.start()):
            raise RuntimeError

    def stop(self):
        dring_cpp.fini()

    def poll_events(self):
        dring_cpp.pollEvents()

    def version(self):
        return dring_cpp.version().decode()

    def callbacks_to_register(self):
        """Returns the python callbacks that will be triggered dring signals.
        The signals are the dict keys.
        """
        global python_callbacks
        return python_callbacks

    def register_callbacks(self, callbacks):
        """Registers the python callbacks received as dict values.
        The corresponding signals are defined as keys.
        Expects the dict with keys defined by the callbacks() method.

        No return
        """
        global python_callbacks
        try:
            for key, value in python_callbacks.items():
                python_callbacks[key] = callbacks[key]
        except KeyError as e:
            raise KeyError("KeyError: %s. You can't change the keys." % e)
