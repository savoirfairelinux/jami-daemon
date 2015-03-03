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
from tester import DRingTester

def printAccountDetails(account):
    details = ctrl.getAccountDetails(account)
    print(account)
    for k in sorted(details.keys()):
        print("  %s: %s" % (k, details[k]))
    print()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument('--gaa', help='Get all accounts (of optionaly given type)',
                        nargs='?', metavar='<type>', type=str, default=argparse.SUPPRESS)
    parser.add_argument('--gara', help='Get all registered accounts', action='store_true')
    parser.add_argument('--gaea', help='Get all enabled accounts', action='store_true')
    parser.add_argument('--gaad', help='Get all account details', action='store_true')

    parser.add_argument('--gac', help='Get all codecs', action='store_true')

    parser.add_argument('--gad', help='Get account details',
                        metavar='<account>', type=str)

    group = parser.add_mutually_exclusive_group()
    group.add_argument('--enable', help='Enable the account',
                       metavar='<account>', type=str)
    group.add_argument('--disable', help='Disable the account',
                       metavar='<account>', type=str)

    group = parser.add_mutually_exclusive_group()
    group.add_argument('--register', help='Register the account',
                       metavar='<account>', type=str)
    group.add_argument('--unregister', help='Unregister the account',
                       metavar='<account>', type=str)

    parser.add_argument('--sac', help='Set active account',
                        metavar='<account>', type=str)

    parser.add_argument('--gacl', help='Get active codecs for the account',
                        nargs='?', metavar='<account>', type=str, default=argparse.SUPPRESS)
    parser.add_argument('--sacl', help='Set active codecs for active account',
                        metavar='<codec list>', type=str)

    #parser.add_argument('--gcc', help='Get current callid', action='store_true')
    parser.add_argument('--gcl', help='Get call list', action='store_true')

    group = parser.add_mutually_exclusive_group()
    group.add_argument('--call', help='Call to number', metavar='<destination>')
    #group.add_argument('--transfer', help='Transfer active call', metavar='<destination>')

    group = parser.add_mutually_exclusive_group()
    group.add_argument('--accept', help='Accept the call', metavar='<account>')
    group.add_argument('--hangup', help='Hangup the call', metavar='<account>')
    group.add_argument('--refuse', help='Refuse the call', metavar='<account>')

    group = parser.add_mutually_exclusive_group()
    group.add_argument('--hold', help='Hold the call', metavar='<call>')
    group.add_argument('--unhold', help='Unhold the call', metavar='<call>')

    parser.add_argument('--dtmf', help='Send DTMF', metavar='<key>')

    parser.add_argument('--test', help='Launch automatic tests', action='store_true')

    args = parser.parse_args()

    ctrl = DRingCtrl(sys.argv[0])
    tester = DRingTester(sys.argv[0])

    if len(sys.argv) == 1:
        ctrl.run()
        sys.exit(0)

    if args.gac:
        print(ctrl.getAllCodecs())

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
            printAccountDetails(account)

    if args.sac:
        ctrl.setAccount(args.sac)

    if args.gad:
        printAccountDetails(args.gad)

    if hasattr(args, 'gacl'):
        print(ctrl.getActiveCodecs(args.gacl))

    if args.sacl:
        ctrl.setActiveCodecList(codec_list=args.sacl)

    if args.enable:
        ctrl.setAccountEnable(args.enable, True)

    if args.disable:
        ctrl.setAccountEnable(args.enable, False)

    if args.register:
        ctrl.setAccountRegistered(args.register, True)

    if args.unregister:
        ctrl.setAccountRegistered(args.unregister, False)

    if args.gcl:
        for call in ctrl.getAllCalls():
            print(call)

    if args.call:
        ctrl.Call(args.call)

    if args.accept:
        ctrl.Accept(args.accept)

    if args.refuse:
        ctrl.Refuse(args.refuse)

    if args.hangup:
        ctrl.HangUp(args.hangup)

    if args.hold:
        ctrl.Hold(args.hold)

    if args.unhold:
        ctrl.UnHold(args.unhold)

    if args.dtmf:
        ctrl.Dtmf(dtmf)
    if args.test:
        tester.start(ctrl)

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
