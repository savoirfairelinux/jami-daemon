#!/usr/bin/env python
#
# Copyright (C) 2009 by the Free Software Foundation, Inc.
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

import gobject
from gobject import GObject

import getopt

import time
import md5

from threading import Thread

from Errors import *

try:
	import dbus
	from dbus.mainloop.glib import DBusGMainLoop
except ImportError, e:
	raise SflPhoneError("No python-dbus module found")


class SflPhoneCtrlSimple(object):
    """Simple class for controlling SflPhoned through DBUS"""

    # list of active calls (known by the client)
    activeCalls = {}

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
            # register the main loop for d-bus events
            DBusGMainLoop(set_as_default=True)
            self.bus = dbus.SessionBus()
        except dbus.DBusException, e:
            raise SPdbusError("Unable to connect DBUS session bus")

        dbus_objects = dbus.Interface(self.bus.get_object(
              'org.freedesktop.DBus', '/org/freedesktop/DBus'), 
                      'org.freedesktop.DBus').ListNames()

        if not "org.sflphone.SFLphone" in dbus_objects:
            raise SPdbusError("Unable to find org.sflphone.SFLphone in DBUS. Check if sflphoned is running")

        try:
            proxy_instance = self.bus.get_object("org.sflphone.SFLphone",
		 "/org/sflphone/SFLphone/Instance", introspect=False)
            proxy_callmgr = self.bus.get_object("org.sflphone.SFLphone",
		 "/org/sflphone/SFLphone/CallManager", introspect=False)
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
            
            raise SPdbusError("Unable to bind to sflphoned api, 
                ask core-dev team to implement getVersion method and 
                      start to pray.")

        try:
            self.instance.Register(os.getpid(), self.name)
            self.registered = True
        except:
            raise SPdaemonError("Client registration failed")

        try:
            proxy_callmgr.connect_to_signal('incomingCall', self.onIncomingCall)
            proxy_callmgr.connect_to_signal('callStateChanged', self.onCallStateChanged)
        except dbus.DBusException, e:
            print e
			

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
    # Signal handling
    #

	# On incoming call event, add the call to the list of active calls
	def onIncomingCall(self, account, callid, to):
		print "Incoming call: " + account + ", " + callid + ", " + to
		self.activeCalls[callid] = {'Account': account, 'To': to, 'State': '' }

	# On call state changed event, set the values for new calls, or delete the call from the list of active calls
	def onCallStateChanged(self, callid, state):
		print "Call state changed: " + callid + ", " + state
		if state == "HUNGUP":
			try:
				del self.activeCalls[callid]
			except KeyError:
				print "Call " + callid + " didn't exist. Cannot delete."
		elif state in [ "RINGING", "CURRENT", "INCOMING", "HOLD" ]:
			try:
				self.activeCalls[callid]['State'] = state 
			except KeyError, e:
				print "This call didn't exist!: " + callid + ". Adding it to the list."
				callDetails = self.getCallDetails(callid)
				self.activeCalls[callid] = {'Account': callDetails['ACCOUNTID'], 'To': callDetails['PEER_NUMBER'], 'State': state }
		elif state in [ "BUSY", "FAILURE" ]:
			try:
				del self.activeCalls[callid]
			except KeyError, e:
				print "This call didn't exist!: " + callid

#		elif state == "UNHOLD_CURRENT":
#			self.activeCalls[callid]['State'] = "UNHOLD_CURRENT"


	#
	# Account management
	#
	def getAllAccounts(self):
		""" Return a list with all accounts"""
		return self.configurationmanager.getAccountList()


	def getAllEnabledAccounts(self):
		""" Return a list with all enabled accounts"""
		accounts = self.getAllAccounts()
		activeaccounts = []
		for testedaccount in accounts:
			if self.isAccountEnable(testedaccount):
				activeaccounts.append(testedaccount)
		return activeaccounts


	def getAccountDetails(self, account=None):
		"""Return a list of string. If no account is provided, active account is used"""

		if account is None:
			if self.account is None:
				raise SflPhoneError("No provided or current account !")
			if checkAccountExists(self.account):
				return self.configurationmanager.getAccountDetails(self.account)
		else:
			if self.checkAccountExists(account):
				return self.configurationmanager.getAccountDetails(account)


	def setAccountByAlias(self, alias):
		"""Define as active the first account who match with the alias"""

		for testedaccount in self.getAllAccounts():
			details = self.getAccountDetails(testedaccount)
			if ( details['Account.enable'] == "TRUE" and details['Account.alias'] == alias ):
				self.account = testedaccount
				return
		raise SPaccountError("No enabled account matched with alias")


	def getAccountByAlias(self, alias):
		"""Get account name having its alias"""

		for account in self.getAllAccounts():
			details = self.getAccountDetails(account)
			if details['Account.alias'] == alias:
				return account
		raise SPaccountError("No account matched with alias")

	def setAccount(self, account):
		"""Define the active account"""

		if account in self.getAllAccounts():
			self.account = account
		else:
			raise SflPhoneError("Not a valid account")

	def setFirstRegisteredAccount(self):
		"""Find the first enabled account and define it as active"""

		rAccounts = self.getAllRegisteredAccounts()
		if 0 == len(rAccounts):
			raise SflPhoneError("No registered account !")
		self.account = rAccounts[0]

	def setFirstActiveAccount(self):
		"""Find the first enabled account and define it as active"""

		aAccounts = self.getAllEnabledAccounts()
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
		return self.getAccountDetails(account)['Status'] == "REGISTERED"


	def isAccountEnable(self, account=None):
		"""Return True if the account is enabled. If no account is provided, active account is used"""

		if account is None:
			if self.account is None:
				raise SflPhoneError("No provided or current account !")
			account = self.account
		return self.getAccountDetails(account)['Account.enable'] == "TRUE"

	def setAccountEnable(self, account=None, enable=False):
		"""Set account enabled"""
		if account is None:
			if self.account is None:
				raise SflPhoneError("No provided or current account !")
			account = self.account

		if enable == True:
			details = self.getAccountDetails(account)
			details['Account.enable'] = "TRUE"
			self.configurationmanager.setAccountDetails(account, details)
		else:
			details = self.getAccountDetails(account)
			details['Account.enable'] = "FALSE"
			self.configurationmanager.setAccountDetails(account, details)

	def checkAccountExists(self, account=None):
		""" Checks if the account exists """
		if account is None:
			raise SflPhoneError("No provided or current account !")
		return account in self.getAllAccounts()
			
	def getAllRegisteredAccounts(self):
		"""Return a list of registered accounts"""

		registeredAccountsList = []
		for account in self.getAllAccounts():
			if self.isAccountRegistered(account):
				registeredAccountsList.append(account)

		return registeredAccountsList

	def getAllEnabledAccounts(self):
		"""Return a list of enabled accounts"""

		enabledAccountsList = []
		for accountName in self.getAllAccounts():
			if  self.getAccountDetails(accountName)['Account.enable'] == "TRUE":
				enabledAccountsList.append(accountName)

		return enabledAccountsList

	def getAllSipAccounts(self):
		"""Return a list of SIP accounts"""

		sipAccountsList = []
		for accountName in self.getAllAccounts(): 
			if  self.getAccountDetails(accountName)['Account.type'] == "SIP":
				sipAccountsList.append(accountName)

		return sipAccountsList

	def getAllIaxAccounts(self):
		"""Return a list of IAX accounts"""

		iaxAccountsList = []
		for accountName in self.getAllAccounts():
			if  self.getAccountDetails(accountName)['Account.type'] == "IAX":
				iaxAccountsList.append(accountName)

		return iaxAccountsList

	def setAccountRegistered(self, account=None, register=False):
		""" Tries to register the account """

		if account is None:
			if self.account is None:
				raise SflPhoneError("No provided or current account !")
			account = self.account

		try:
			if register:
				self.configurationmanager.sendRegister(account, int(1))
				#self.setAccount(account)
			else:
				self.configurationmanager.sendRegister(account, int(0))
				#self.setFirstRegisteredAccount()

		except SflPhoneError, e:
			print e

        #
        # Codec manager
        #
                        
        def getCodecList(self):
                """ Return the codec list """
                return self.configurationmanager.getCodecList()

        def getActiveCodecList(self):
                """ Return the active codec list """
                return self.configurationmanager.getActiveCodecList()



	#
	# Call management
	#

	def getCurrentCallID(self):
		"""Return the callID of the current call if any"""

		return self.callmanager.getCurrentCallID()


	def getCurrentCallDetails(self):
		"""Return informations on the current call if any"""

		return self.callmanager.getCallDetails(self.getCurrentCallID())

	def getCallDetails(self, callid):
		"""Return informations on this call if exists"""

		return self.callmanager.getCallDetails(callid)

	def printClientCallList(self):
		print "Client active call list:"
		print "------------------------"
		for call in self.activeCalls:
			print "\t" + call

	#
	# Action
	#
	def Call(self, dest):
		"""Start a call and return a CallID"""
		if not self.account:
			self.setFirstRegisteredAccount()

		if not self.isAccountRegistered():
			raise SflPhoneError("Can't place a call without a registered account")

		if dest is None or dest == "":
			raise SflPhoneError("Invalid call destination")

#		callid = str(random.randrange(2**32-1))
		t = long( time.time() * 1000 )
		r = long( random.random()*100000000000000000L )
		data = str(t) + str(r)
		callid = md5.md5(data).hexdigest()

		# Add the call to the list of active calls and set status to SENT
		self.activeCalls[callid] = {'Account': self.account, 'To': dest, 'State': 'SENT' }
		# Send the request to the CallManager
		self.callmanager.placeCall(self.account, callid, dest)

		return callid


	def HangUp(self, callid):
		"""End a call identified by a CallID"""
		if not self.account:
			self.setFirstRegisteredAccount()

		if not self.isAccountRegistered():
			raise SflPhoneError("Can't hangup a call without a registered account")

		if callid is None or callid == "":
			pass # just to see
			#raise SflPhoneError("Invalid callID")

		self.callmanager.hangUp(callid)


	def Transfert(self, callid, to):
		"""Transfert a call identified by a CallID"""
		if not self.account:
			self.setFirstRegisteredAccount()

		if not self.isAccountRegistered():
			raise SflPhoneError("Can't transfert a call without a registered account")

		if callid is None or callid == "":
			raise SflPhoneError("Invalid callID")

		self.callmanager.transfert(callid, to)


	def Refuse(self, callid):
		"""Refuse an incoming call identified by a CallID"""
		if not self.account:
			self.setFirstRegisteredAccount()

		if not self.isAccountRegistered():
			raise SflPhoneError("Can't refuse a call without a registered account")

		if callid is None or callid == "":
			raise SflPhoneError("Invalid callID")

		self.callmanager.refuse(callid)


	def Accept(self, callid):
		"""Accept an incoming call identified by a CallID"""
		if not self.account:
			self.setFirstRegisteredAccount()

		if not self.isAccountRegistered():
			raise SflPhoneError("Can't accept a call without a registered account")

		if callid is None or callid == "":
			raise SflPhoneError("Invalid callID")

		self.callmanager.accept(callid)


	def Hold(self, callid):
		"""Hold a call identified by a CallID"""
		if not self.account:
			self.setFirstRegisteredAccount()

		if not self.isAccountRegistered():
			raise SflPhoneError("Can't hold a call without a registered account")

		if callid is None or callid == "":
			raise SflPhoneError("Invalid callID")

		self.callmanager.hold(callid)


	def UnHold(self, callid):
		"""Unhold an incoming call identified by a CallID"""
		if not self.account:
			self.setFirstRegisteredAccount()

		if not self.isAccountRegistered():
			raise SflPhoneError("Can't unhold a call without a registered account")

		if callid is None or callid == "":
			raise SflPhoneError("Invalid callID")

		self.callmanager.unhold(callid)


	def Dtmf(self, key):
		"""Send a DTMF"""
		self.callmanager.playDTMF(key)



