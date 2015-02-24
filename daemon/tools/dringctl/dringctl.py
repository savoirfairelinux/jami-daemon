#!/usr/bin/env python3
#
# Copyright (C) 2015 Savoir-Faire Linux Inc.
# Author: Guillaume Roguez <guillaume.roguez@savoirfairelinux.com>
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
# Additional permission under GNU GPL version 3 section 7:
#
# If you modify this program, or any covered work, by linking or
# combining it with the OpenSSL project's OpenSSL library (or a
# modified version of that library), containing parts covered by the
# terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
# grants you additional permission to convey the resulting work.
# Corresponding Source for a non-source form of such a combination
# shall include the source code for the parts of OpenSSL used as well
# as that of the covered work.
#

import sys
import os
import random
import time
import argparse

from gi.repository import GObject

from errors import *
from controler import DRingCtrl


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--gaa', help='Get all accounts (of optionaly given type)',
                        nargs='?', metavar='<type>', type=str, default=argparse.SUPPRESS)
    parser.add_argument('--gara', help='Get all registered accounts', action='store_true')
    parser.add_argument('--gaea', help='Get all enabled accounts', action='store_true')

    parser.add_argument('--gaad', help='Get all account details', action='store_true')

    parser.add_argument('--gac', help='Get all codecs', action='store_true')

    #parser.add_argument('--gad', help='Get account details', metavar='<account>')
    #parser.add_argument('--enable', help='Enable the account', metavar='<account>')
    #parser.add_argument('--disable', help='Disable the account', metavar='<account>')
    #parser.add_argument('--register', help='Register the account', metavar='<account>')
    #parser.add_argument('--unregister', help='Unregister the account', metavar='<account>')
    #parser.add_argument('--sac', help='Set accout for next call', metavar='<account>')

    #parser.add_argument('--gcc', help='Get current callid', action='store_true')
    #parser.add_argument('--call', help='Call to number', metavar='<destination>')
    #parser.add_argument('--transfer', help='Transfer active call', metavar='<destination>')
    #parser.add_argument('--gcd', help='Get call details', metavar='<account|"current">')
    #parser.add_argument('--accept', help='Accept the call', metavar='<account|"current">')
    #parser.add_argument('--hangup', help='Hangup the call', metavar='<account|"current">')
    #parser.add_argument('--refuse', help='Refuse the call', metavar='<account|"current">')
    #parser.add_argument('--hold', help='Hold the call', metavar='<account|"current">')
    #parser.add_argument('--unhold', help='Unhold the call', metavar='<account|"current">')
    #parser.add_argument('--dtmf', help='Send DTMF', metavar='<key>')

    args = parser.parse_args()

    ctrl = DRingCtrl(sys.argv[0])

    if hasattr(args, 'gaa'):
        for account in ctrl.getAllAccounts(args.gaa):
            print(account)

    if args.gara:
        for account in ctrl.getAllRegisteredAccounts():
            print(account)

    if args.gaea:
        for account in ctrl.getAllEnabledAccounts():
            print(account)

    if args.gaad:
        for account in ctrl.getAllAccounts():
            details = ctrl.getAccountDetails(account)
            print(account)
            for k in sorted(details.keys()):
                print("  %s: %s" % (k, details[k]))
            print()

    if args.gac:
        for codec in ctrl.getAllCodecs():
            print(codec)

"""

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


"""
