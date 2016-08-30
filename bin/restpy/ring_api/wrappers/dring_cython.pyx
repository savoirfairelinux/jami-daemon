# cython: language_level=3
#
# Copyright (C) 2016 Savoir-faire Linux Inc
#
# Authors: Seva Ivanov <seva.ivanov@savoirfairelinux.com>
#          Simon Zeni <simon.zeni@savoirfairelinux.com>
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

from libc.stdint cimport *
from libcpp.string cimport string
from libcpp.map cimport map as map
from libcpp.vector cimport vector

from ring_api.utils.std cimport *
from ring_api.utils.cython import *

from ring_api.interfaces cimport dring as dring_cpp
from ring_api.interfaces cimport configuration_manager as confman_cpp
from ring_api.interfaces cimport call_manager as callman_cpp
from ring_api.interfaces cimport video_manager as videoman_cpp
from ring_api.interfaces cimport cb_client as cb_client_cpp

# python callbacks
global py_cbs
py_cbs = dict.fromkeys(['account_message'])

# python callbacks context
global py_cbs_ctx

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

    global py_cbs_ctx
    global py_cbs
    callback = py_cbs['account_message']

    if (callback and py_cbs_ctx):
        callback(py_cbs_ctx, str(account_id), str(from_ring_id), content)
    elif (callback):
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

    def account_details(self, account_id):
        """Gets account details

        Keyword argument:
        account_id -- account id string

        Return: account_details dict
        """
        cdef string raw_id = account_id.encode()

        return raw_dict_to_dict(confman_cpp.getAccountDetails(raw_id))

    def account_volatile_details(self, account_id):
        """Gets account details

        Keyword argument:
        account_id -- account id string

        Return: account_details dict
        """
        cdef string raw_id = account_id.encode()

        return raw_dict_to_dict(confman_cpp.getVolatileAccountDetails(raw_id))

    def set_details(self, account_id, details):
        """Set account details

        Keyword argument:
        account_id -- account id string
        details -- account details dict

        Return: account_details dict
        """
        cdef string raw_id = account_id.encode()
        cdef map[string, string] raw_details

        for key in details:
            raw_details[key.encode()] = details[key].encode()

        confman_cpp.setAccountDetails(raw_id, raw_details)

    def set_account_active(self, account_id, active):
        """Activates the account

        Keyword arguments:
        account_id   -- account id string
        active       -- status bool
        """
        confman_cpp.setAccountActive(account_id.encode(), active)

    def get_account_template(self, account_type):
        """Generates a template for the account

        Keyword argument:
        account_type -- account type string (SIP, RING)

        Return: template dict
        """
        raw_template = confman_cpp.getAccountTemplate(account_type.encode())

        return raw_dict_to_dict(raw_template)

    def add_account(self, details):
        """Adds a new account

        Keyword argument:
        details -- account details template dict

        Return: account_id string
        """
        cdef map[string, string] raw_details

        for key in details:
            raw_details[key.encode()] = details[key].encode()

        cdef string raw_account_id = confman_cpp.addAccount(raw_details)

        return raw_account_id.decode()

    def remove_account(self, account_id):
        """Removes an account from the daemon

        Keywork argument:
        account_id -- account id string

        No return
        """
        confman_cpp.removeAccount(account_id.encode())

    def accounts(self):
        """Lists the user accounts by account id (ring_id != account_id)

        Return: accounts list
        """
        return raw_list_to_list(confman_cpp.getAccountList())

    def send_account_message(self, account_id, ring_id, content):
        """Sends account message

        Keyword arguments:
        account_id  -- account id string
        ring_id     -- ring id destination string
        content     -- dict of content defined as {<mime-type>: <message>}

        Return: success int (0 = Failure, int = message_id)
        """
        cdef string raw_account_id = account_id.encode()
        cdef string raw_ring_id = ring_id.encode()
        cdef map[string, string] raw_content

        for key, value in content.iteritems():
            raw_content[key.encode()] = value.encode()

        return confman_cpp.sendAccountTextMessage(
            raw_account_id, raw_ring_id, raw_content)

    def get_message_status(self, message_id):
        """Gets account message

        Keyword arguments:
        message_id  -- message id int

        Return: message status int
        (0 = UNKNOWN, 1 = SENDING, 2 = SENT, 3 = READ, 4 = FAILURE)
        """
        cdef uint64_t raw_message_id = message_id
        return confman_cpp.getMessageStatus(raw_message_id)

    def get_tls_default_settings(self):
        """Get the TLS default settings

        Return: default settings dict
        """
        return raw_dict_to_dict(confman_cpp.getTlsDefaultSettings())

    def get_supported_ciphers(self, account_id):
        """Gets the supported ciphers for an account

        Keyword arguments:
        account_id  -- account id int

        Return: supported ciphers list
        """

        return raw_list_to_list(
                  confman_cpp.getSupportedCiphers(account_id.encode())
               )

    def get_codec_list(self):
        """Get the list of codecs

        Return: codecs list
        """
        return confman_cpp.getCodecList()

    def get_supported_tls_method(self):
        """Get the list of TLS supported methods

        Return: methods list
        """
        return raw_list_to_list(confman_cpp.getSupportedTlsMethod())

    def get_codec_details(self, account_id, codec_id):
        """Get the details of a codec for an accounts

        account_id -- account id string
        codec_id -- codec id string

        Return: codec details list
        """
        return raw_dict_to_dict(
            confman_cpp.getCodecDetails(account_id.encode(), codec_id)
        )

    def set_codec_details(self, account_id, codec_id, details):
        """Set the details of a codec for an accounts

        account_id -- account id string
        codec_id -- codec id string
        details -- codec details list

        Return: codec details list
        """
        cdef map[string, string] raw_details

        for key, value in details.iteritems():
            raw_details[key.encode()] = value.encode()

        return confman_cpp.setCodecDetails(
            account_id.encode(),
            codec_id,
            raw_details
        )

    def get_active_codec_list(self, account_id):
        """Get the active codec list for an accounts

        account_id -- account id string

        Return: active codec list
        """
        return confman_cpp.getActiveCodecList(account_id.encode())

    def set_active_codec_list(self, account_id, codec_list):
        """Set the active codec list for an accounts

        account_id -- account id string
        codec_list -- codec list string

        Return: codec details list
        """
        confman_cpp.setActiveCodecList(account_id.encode(), codec_list)

    def get_audio_plugin_list(self):
        """Gets the list of audio plugin

        Return: plugin list
        """
        return raw_list_to_list(confman_cpp.getAudioPluginList())

    def validate_certificate(self, account_id, certificate):
        """Validate a certificaty by it's id

        Keyword arguments:
        account_id  -- account id string
        certificate -- certificate string

        Return: list of all certificate validation
        """
        raw_valid_certif = confman_cpp.validateCertificate(
            account_id.encode(),
            certificate.encode()
        )

        return raw_dict_to_dict(raw_valid_certif)

    def get_certificate_details(self, certificate):
        """Gets the certificate details

        Keyword argument:
        certificate -- certificate string

        Return: dict of certificate details
        """
        return raw_dict_to_dict(
            confman_cpp.getCertificateDetails(certificate.encode())
        )

    def get_pinned_certificates(self):
        """Gets all known certificate IDs

        Return: list of pinned certificates
        """
        return raw_list_to_list(confman_cpp.getPinnedCertificates())

    def pin_certificate(self, certificate, local):
        """Pin a certificate to the daemon

        Keyword arguments:
        certificate -- certificate string
        local       -- save the certificate in the local storage bool

        Return: list of ids of the pinned certificate
        """

        cdef vector[uint8_t] raw_cert

        cert_list = [ord(x) for x in list(certificate)]

        for x in cert_list:
            raw_cert.append(x)

        return confman_cpp.pinCertificate(raw_cert, local)

    def unpin_certificate(self, cert_id):
        """Unpin a certificate from the daemon

        Keyword arguments:
        cert_id     -- certificate id string

        Return: true if the certificate was unpinned
        """
        return confman_cpp.unpinCertificate(cert_id.encode())

    def pin_remote_certificate(self, account_id, ring_id):
        """Pin a certificate for a user from a ring ID

        Keyword arguments:
        account_id  -- account id string
        ring_id     -- ring id string

        Return: boolean of the operation success
        """
        return confman_cpp.pinRemoteCertificate(
            account_id.encode(),
            ring_id.encode()
        )

    def set_certificate_status(self, account_id, cert_id, status):
        """Sets the status of an account certificate

        Keyword arguments:
        account_id  -- account id string
        cert_id     -- certificate id string
        status      -- status string (UNDEFINED, ALLOWED, BANNED)

        Return: boolean of the operation success
        """
        return confman_cpp.setCertificateStatus(
            account_id.encode(),
            cert_id.encode(),
            status.encode()
        )

cdef class VideoManager:

    def devices(self):
        """Lists the available video devices

        Return: list of devices
        """
        return raw_list_to_list(videoman_cpp.getDeviceList())

    def get_settings(self, name):
        """Gets the settings of a given device

        Keyword arguments:
        name      -- device id string

        Return: dict of settings
        """
        return raw_dict_to_dict(videoman_cpp.getSettings(name.encode()))

    def apply_settings(self, name, settings):
        """Changes the settings of a given device

        Keyword arguments:
        name      -- device id string
        settings  -- settings dict
        """
        cdef map[string, string] raw_settings

        for key, value in settings.iteritems():
            raw_settings[key.encode()] = value.encode()

        videoman_cpp.applySettings(name.encode(), raw_settings)

    def set_default_device(self, dev):
        videoman_cpp.setDefaultDevice(dev.encode())

    def get_default_device(self):
        return videoman_cpp.getDefaultDevice().decode()

    def start_camera(self):
        videoman_cpp.startCamera()

    def stop_camera(self):
        videoman_cpp.stopCamera()

    def switch_input(self, resource):
        """Changes the input of the video stream

        Currently, the following are supported:
            - camera://DEVICE
            - display://DISPLAY_NAME[ WIDTHxHEIGHT]
            - file://IMAGE_PATH

        Keyword arguments:
        resource  -- string

        Return: True if the input stream was successfully changed
        """
        return videoman_cpp.switchInput(resource.encode())

    def has_camera_started(self):
        return videoman_cpp.hasCameraStarted()

cdef class CallManager:

    def place_call(self, account_id, to):
        """Places a call between two users

        Keyword argument:
        account_id  -- account string
        to          -- ring_id string

        Return: string of the call_id
        """

        raw_call_id = callman_cpp.placeCall(account_id.encode(), to.encode())

        return raw_call_id.decode()

    def refuse(self, call_id):
        """Refuses an incoming call

        Keyword argument:
        call_id -- call id string

        Return: boolean of operation success
        """
        return callman_cpp.refuse(call_id.encode())

    def accept(self, call_id):
        """Accepts an incoming call

        Keyword argument:
        call_id -- call id string

        Return: boolean of operation success
        """
        return callman_cpp.accept(call_id.encode())

    def hang_up(self, call_id):
        """Hangs up a call which is in state (CURRENT, HOLD)

        Keyword argument:
        call_id -- call id string

        Return: boolean of operation success
        """
        return callman_cpp.hangUp(call_id.encode())

    def hold(self, call_id):
        """Places a call which is in state (HOLD)

        Keyword argument:
        call_id -- call id string

        Return: boolean of operation success
        """
        return callman_cpp.hold(call_id.encode())

    def unhold(self, call_id):
        """Takes a call from (HOLD) and place it in (CURRENT) state

        Keyword argument:
        call_id -- call id string

        Return: boolean of operation success
        """
        return callman_cpp.unhold(call_id.encode())


cdef class PresenceManager:
    pass

cdef class Dring:
    cdef:
        readonly int _FLAG_DEBUG
        readonly int _FLAG_CONSOLE_LOG
        readonly int _FLAG_AUTOANSWER

    cdef public ConfigurationManager config
    cdef public VideoManager video
    cdef public CallManager call
    cdef public PresenceManager pres
    cdef CallbacksClient cb_client

    def __cinit__(self):
        self._FLAG_DEBUG = dring_cpp.DRING_FLAG_DEBUG
        self._FLAG_CONSOLE_LOG = dring_cpp.DRING_FLAG_CONSOLE_LOG
        self._FLAG_AUTOANSWER = dring_cpp.DRING_FLAG_AUTOANSWER

        self.config = ConfigurationManager()
        if (not self.config):
            raise RuntimeError

        self.video = VideoManager()
        if (not self.video):
            raise RuntimeError

        self.call = CallManager()
        if (not self.video):
            raise RuntimeError

        self.pres = PresenceManager()
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
        """Returns a dict for callbacks to register as keys values"""
        global py_cbs
        return py_cbs

    def register_callbacks(self, callbacks, context=None):
        """Registers the python callbacks received as dict values.

        Keyword argument:
        callbacks -- dict of functions to callback
                     the corresponding signals are defined as keys
                     keys immutable and defined by the callbacks() method
        context -- context which will be passed as argument to the callbacks

        No return
        """
        global py_cbs
        try:
            for key, value in py_cbs.items():
                py_cbs[key] = callbacks[key]
        except KeyError as e:
            raise KeyError("KeyError: %s. You can't change the keys." % e)

        global py_cbs_ctx
        py_cbs_ctx = context
