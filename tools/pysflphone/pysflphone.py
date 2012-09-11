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



import sys
import os
import random
from traceback import print_exc

import gobject
from gobject import GObject

import getopt

import time

from threading import Thread

from Errors import *

try:
	import dbus
	from dbus.mainloop.glib import DBusGMainLoop
except ImportError, e:
	raise SflPhoneError("No python-dbus module found")


from sflphonectrl import SflPhoneCtrl

#
# Main application
#


def printHelp():
	"""Print help"""
	print sys.argv[0] + " usage:\n\
\n\
	--help                             Print this help.\n\
\n\
	--gaa                              Get all accounts.\n\
	--gara                             Get all registered accounts.  \n\
	--gaea                             Get all enabled accounts.     \n\
	--gasa                             Get all SIP accounts.         \n\
	--gaia                             Get all IAX accounts.         \n\
	--gcc                              Get current callid.           \n\
	--gacl                             Get active codec list.        \n\
        --sac                              Set accout for next call      \n\
                                                                         \n\
	--gad        <account>             Get account details .         \n\
	--enable     <account>             Enable the account.           \n\
	--disable    <account>             Disable the account.          \n\
	--register   <account>             Register the account.         \n\
	--unregister <account>             Unregister the account.       \n\
	                                                                 \n\
	--call       <destination>         Call to number                \n\
	--transfer   <destination>         Transfer active call          \n\
\n\
	--gcd        <callid|\"current\">    Get call details.           \n\
	--accept     <callid|\"current\">    Accept the call             \n\
	--hangup     <callid|\"current\">    Hangup the call             \n\
	--refuse     <callid|\"current\">    Refuse the call             \n\
	--hold       <callid|\"current\">    Hold the call               \n\
	--unhold     <callid|\"current\">    Unhold the call             \n\
	--dtmf       <key>                 Send DTMF\n"
	


# Option definition
try:
    opts, args =  getopt.getopt(sys.argv[1:],"",
				[  "help", "gaa", "gal", "gara", "gaea", "gasa", "gaia",
				   "gacl", "gac", "gcc", "hangup=", "refuse=", "hold",
				   "unhold=", "transfer=","dtmf=", "accept=", "gcd=",
				   "gad=", "register=", "unregister=", "enable=", "disable=",
				   "call=", "sac=" ])
except getopt.GetoptError,err:
    print str(err)
    sys.exit(2)


# SFLPhone instance.
sflphone = SflPhoneCtrl()

# If no arguments, run the d-bus event loop.
if len(sys.argv) == 1:
	loop = gobject.MainLoop()
	loop.run()

# Parse all arguments
else:
	for opt, arg in opts:	

		if opt == "--help":
			printHelp()

		#
		# info options
		#

		# Get all accounts
		elif opt == "--gaa":
			for account in sflphone.getAllAccounts():
				print account

		# Get all registered accounts
		elif opt == "--gara":
			for account in sflphone.getAllRegisteredAccounts():
				print account

		# Get all enabled accounts
		elif opt == "--gaea":
			for account in sflphone.getAllEnabledAccounts():
				print account

		# Get all SIP accounts
		elif opt == "--gasa":
			for account in sflphone.getAllSipAccounts():
				print account

		# Get all IAX accounts
		elif opt == "--gaia":
			for account in sflphone.getAllIaxAccounts():
				print account

		# Get current call
		elif opt == "--gcc":
			call = sflphone.getCurrentCallID()
			if call:
				print call
			else:
				print "No current call."

		# Get account details
		elif opt == "--gad":
			if sflphone.checkAccountExists(arg):
				details = sflphone.getAccountDetails(arg)
				for var in details:
					print var + ": " + details[var]
			else:
				print "No such account: " + arg

		# Get active codec list
		elif opt == "--gacl":
			print "Not implemented."

		# Get call details
		elif opt == "--gcd":
			if arg == "current": arg = sflphone.getCurrentCallID()

			details = sflphone.getCallDetails(arg)
			if details:
				print "Call: " + arg
				print "Account: " + details['ACCOUNTID']
				print "Peer: " + details['PEER_NAME'] + "<" + details['PEER_NUMBER'] + ">"

		elif opt == "--sac":
			if arg is "":
			    print "Must specifies the accout to be set"
			else:
                            sflphone.setAccount(arg)


		#
		# call options
		#

		# Make a call
		elif opt == "--call":
			sflphone.Call(arg)

		# Hangup a call
		elif opt == "--hangup":
			if arg == "current":
				arg = sflphone.getCurrentCallID()

			if arg:
				sflphone.HangUp(arg)

		# Refuse a call
		elif opt == "--refuse":
			if arg == "current": arg = sflphone.getCurrentCallID()
			sflphone.Refuse(arg)

		# Hold a call
		elif opt == "--hold":
			if arg == "current": arg = sflphone.getCurrentCallID()
			sflphone.Hold(arg)

		# Unhold a call
		elif opt == "--unhold":
			if arg == "current": arg = sflphone.getCurrentCallID()
			sflphone.UnHold(arg)

		# Transfer the current call
		elif opt == "--transfer":
			call = sflphone.callmanager.getCurrentCallID()
			sflphone.Transfert(call, arg)

		# Send DTMF
		elif opt == "--dtmf":
			sflphone.Dtmf(arg)

		# Accept a call
		elif opt == "--accept":
			if arg == "current": arg = sflphone.getCurrentCallID()
			sflphone.Accept(arg)


		#
		# account options
		#

		# Register an account
		elif opt == "--register":
			if not sflphone.checkAccountExists(arg):
				print "Account " + arg + ": no such account."

			elif arg  in sflphone.getAllRegisteredAccounts():
				print "Account " + arg + ": already registered."

			else:
				sflphone.setAccountRegistered(arg, True)
				print arg + ": Sent register request."

		# Unregister an account
		elif opt == "--unregister":
			if not sflphone.checkAccountExists(arg):
				print "Account " + arg + ": no such account."

			elif arg not in sflphone.getAllRegisteredAccounts():
				print "Account " + arg + ": is not registered."

			else:
				sflphone.setAccountRegistered(arg, False)
				print arg + ": Sent unregister request."

		# Enable an account
		elif opt == "--enable":
			if not sflphone.checkAccountExists(arg):
				print "Account " + arg + ": no such account."

			elif sflphone.isAccountEnable(arg):
				print "Account " + arg + ": already enabled."

			else:
				sflphone.setAccountEnable(arg, True)
				print arg + ": Account enabled."

		# Disable an account
		elif opt == "--disable":
			if not sflphone.checkAccountExists(arg):
				print "Account " + arg + ": no such account."

			elif not sflphone.isAccountEnable(arg):
				print "Account " + arg + ": already disabled."

			else:
				sflphone.setAccountRegistered(arg, False)
				sflphone.setAccountEnable(arg, False)
				print arg + ": Account disabled."


