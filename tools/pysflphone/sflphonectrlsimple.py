#!/usr/bin/env python
#
# Copyright (C) 2008 by the Free Software Foundation, Inc.
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

"""Simple class for controlling SflPhoned through DBUS"""

import sys
import os
import random
from traceback import print_exc

from Errors import *

try:
    import dbus
except ImportError, e:
    raise SflPhoneError("No python-dbus module found")

# TODO list
# - handle signals to stay synchronzed with state of daemon (ie server registration can be lost anytime)


class SflPhoneCtrlSimple(object):
    """Simple class for controlling SflPhoned through DBUS"""


    def __init__(self, name=sys.argv[0]):
        # current active account
        self.account = None
        # client name
        self.name = name
        # client registered to sflphoned ?
        self.registered = False

        self.register()


    def __del__(self):
        if self.registered:
            self.unregister()


    def register(self):
        if self.registered:
            return

        try:
            self.bus = dbus.SessionBus()
        except dbus.DBusException, e:
            raise SPdbusError("Unable to connect DBUS session bus")

        dbus_objects = dbus.Interface(self.bus.get_object('org.freedesktop.DBus', '/org/freedesktop/DBus'), 'org.freedesktop.DBus').ListNames()
        if not "org.sflphone.SFLphone" in dbus_objects:
            raise SPdbusError("Unable to find org.sflphone.SFLphone in DBUS. Check if sflphoned is running")

        try:
            proxy_instance = self.bus.get_object("org.sflphone.SFLphone",
                                                 "/org/sflphone/SFLphone/Instance",
                                                 introspect=False)
            proxy_callmgr = self.bus.get_object("org.sflphone.SFLphone",
                                                "/org/sflphone/SFLphone/CallManager",
                                                introspect=False)
            proxy_confmgr = self.bus.get_object("org.sflphone.SFLphone",
                                                "/org/sflphone/SFLphone/ConfigurationManager",
                                                introspect=False)
            self.instance = dbus.Interface(proxy_instance,
                                           "org.sflphone.SFLphone.Instance")
            self.callmanager = dbus.Interface(proxy_callmgr,
                                              "org.sflphone.SFLphone.CallManager")
            self.configurationmanager = dbus.Interface(proxy_confmgr,
                                                       "org.sflphone.SFLphone.ConfigurationManager")
        except dbus.DBusException, e:
            raise SPdbusError("Unable to bind to sflphoned api, ask core-dev team to implement getVersion method and start to pray.")

        try:
            self.instance.Register(os.getpid(), self.name)
            self.registered = True
        except:
            raise SPdaemonError("Client registration failed")


    def unregister(self):
        if not self.registered:
            return
            #raise SflPhoneError("Not registered !")
        try:
            self.instance.Unregister(os.getpid())
            self.registered = False
        except:
            raise SPdaemonError("Client unregistration failed")


    def isRegistered(self):
        return self.registered

    #
    # Account management
    #
    def getAccountList(self):
        return self.configurationmanager.getAccountList()


    def getEnabledAccountList(self):
        accounts = self.configurationmanager.getAccountList()
        activeaccounts = []
        for testedaccount in accounts:
            if ( self.configurationmanager.getAccountDetails(testedaccount)['Account.enable'] == "TRUE" ):
                activeaccounts.append(testedaccount)
        return activeaccounts


    def getAccountDetails(self, account=None):
        """Return a list of string. If no account is provided, active account is used"""

        if account is None:
            if self.account is None:
                raise SflPhoneError("No provided or current account !")
            return self.configurationmanager.getAccountDetails(self.account)
        else:
            return self.configurationmanager.getAccountDetails(account)


    def setAccountByAlias(self, alias):
        """Define as active the first account who match with the alias"""

        for testedaccount in self.configurationmanager.getAccountList():
            details = self.configurationmanager.getAccountDetails(testedaccount)
            if ( details['Account.enable'] == "TRUE" and details['Account.alias'] == alias ):
                self.account = testedaccount
                return
        raise SPaccountError("No account matched with alias")


    def setAccount(self, account):
        """Define the active account"""

        if account in self.getAccountList():
            self.account = account
        else:
            raise SflPhoneError("Not a valid account")


    def setFirstActiveAccount(self):
        """Find the first enabled account and define it as active"""

        aAccounts = self.getEnabledAccountList()
        if 0 == len(aAccounts):
            raise SflPhoneError("No active account !")
        self.account = aAccounts[0]


    def getAccount(self):
        """Return the active account"""

        return self.account


    def isAccountRegistered(self, account=None):
        """Return True if the account is registered. If no account is provided, active account is used"""

        if account is None:
            if self.account is None:
                raise SflPhoneError("No provided or current account !")
            account = self.account
        return self.configurationmanager.getAccountDetails(account)['Status'] == "REGISTERED"


    def isAccountEnable(self, account=None):
        """Return True if the account is enabled. If no account is provided, active account is used"""

        if account is None:
            if self.account is None:
                raise SflPhoneError("No provided or current account !")
            account = self.account
        return self.configurationmanager.getAccountDetails(account)['Account.enable'] == "TRUE"


    #
    # Call management
    #

    def getCurrentCallID(self):
        """Return the callID of the current call if any"""

        return self.callmanager.getCurrentCallID()


    def getCallDetails(self):
        """Return informations on the current call if any"""

        return self.callmanager.getCallDetails(self.getCurrentCallID())


    #
    # Action
    #
    def Call(self, dest):
        """Start a call and return a CallID"""

        if not self.isAccountRegistered():
            raise SflPhoneError("Can't place a call without a registered account")

        if dest is None or dest == "":
            pass # just to see
            #raise SflPhoneError("Invalid call destination")

        callid = str(random.randrange(2**32-1))
        self.callmanager.placeCall(self.account, callid, dest)
        return callid


    def HangUp(self, callid):
        """End a call identified by a CallID"""

        if not self.isAccountRegistered():
            raise SflPhoneError("Can't hangup a call without a registered account")

        if callid is None or callid == "":
            pass # just to see
            #raise SflPhoneError("Invalid callID")

        self.callmanager.hangUp(callid)


    def Transfert(self, callid):
        """Transfert a call identified by a CallID"""

        if not self.isAccountRegistered():
            raise SflPhoneError("Can't transfert a call without a registered account")

        if callid is None or callid == "":
            raise SflPhoneError("Invalid callID")

        self.callmanager.transfert(callid)


    def Refuse(self, callid):
        """Refuse an incoming call identified by a CallID"""

        if not self.isAccountRegistered():
            raise SflPhoneError("Can't refuse a call without a registered account")

        if callid is None or callid == "":
            raise SflPhoneError("Invalid callID")

        self.callmanager.refuse(callid)


    def Accept(self, callid):
        """Accept an incoming call identified by a CallID"""

        if not self.isAccountRegistered():
            raise SflPhoneError("Can't accept a call without a registered account")

        if callid is None or callid == "":
            raise SflPhoneError("Invalid callID")

        self.callmanager.accept(callid)


    def Hold(self, callid):
        """Hold a call identified by a CallID"""

        if not self.isAccountRegistered():
            raise SflPhoneError("Can't hold a call without a registered account")

        if callid is None or callid == "":
            raise SflPhoneError("Invalid callID")

        self.callmanager.hold(callid)


    def UnHold(self, callid):
        """Unhold an incoming call identified by a CallID"""

        if not self.isAccountRegistered():
            raise SflPhoneError("Can't unhold a call without a registered account")

        if callid is None or callid == "":
            raise SflPhoneError("Invalid callID")

        self.callmanager.unhold(callid)





