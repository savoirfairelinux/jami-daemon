#!/usr/bin/env python3
#
# Copyright (C) 2015 Savoir-faire Linux Inc.
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

    parser.add_argument('--get-all-accounts', help='Get all accounts (of optionaly given type)',
                        nargs='?', metavar='<type>', type=str, default=argparse.SUPPRESS)
    parser.add_argument('--get-registered-accounts', help='Get all registered accounts',
                        action='store_true')
    parser.add_argument('--get-enabled-accounts', help='Get all enabled accounts',
                        action='store_true')
    parser.add_argument('--get-all-accounts-details', help='Get all accounts details',
                        action='store_true')

    parser.add_argument('--add-ring-account', help='Add new Ring account',
                        metavar='<account>', type=str)
    parser.add_argument('--remove-ring-account', help='Remove Ring account',
                        metavar='<account>', type=str)
    parser.add_argument('--get-account-details', help='Get account details',
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
    parser.add_argument('--set-active-account', help='Set active account',
                        metavar='<account>', type=str)

    parser.add_argument('--get-all-codecs', help='Get all codecs', action='store_true')
    parser.add_argument('--get-active-codecs', help='Get active codecs for the account',
                        nargs='?', metavar='<account>', type=str, default=argparse.SUPPRESS)
    parser.add_argument('--get-active-codecs-details', help='Get active codecs details for the account',
                        metavar='<account>',type=str)
    parser.add_argument('--set-active-codecs', help='Set active codecs for active account',
                        metavar='<codec list>', type=str)

    parser.add_argument('--get-call-list', help='Get call list', action='store_true')
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
    parser.add_argument('--toggleVideo', help='Launch toggle video  tests', action='store_true')

    parser.add_argument('--test', help=' '.join(str(test) for test in DRingTester().getTestName() ), metavar='<testName>')

    args = parser.parse_args()

    ctrl = DRingCtrl(sys.argv[0])

    if len(sys.argv) == 1:
        ctrl.run()
        sys.exit(0)

    if args.add-ring-account:
        accDetails = {'Account.type':'RING', 'Account.alias':args.add-ring-account if args.add-ring-account!='' else 'RingAccount'}
        accountID = ctrl.addAccount(accDetails)

    if args.remove-ring-account and args.remove-ring-account != '':
        ctrl.removeAccount(args.remove-ring-account)

    if args.get-all-codecs:
        print(ctrl.getAllCodecs())

    if hasattr(args, 'get-all-accounts'):
        for account in ctrl.getAllAccounts(args.get-all-accounts):
            print(account)

    if args.get-registered-accounts:
        for account in ctrl.getAllRegisteredAccounts():
            print(account)

    if args.get-enabled-accounts:
        for account in ctrl.getAllEnabledAccounts():
            print(account)

    if args.get-all-accounts-details:
        for account in ctrl.getAllAccounts():
            printAccountDetails(account)

    if args.get-active-codecs-details:
        for codecId in ctrl.getActiveCodecs(args.get-active-codecs-details):
            print("# codec",codecId,"-------------")
            print(ctrl.getCodecDetails(args.get-active-codecs-details, codecId))
            print("#-- ")

    if args.set-active-account:
        ctrl.setAccount(args.set-active-account)

    if args.get-account-details:
        printAccountDetails(args.get-account-details)

    if hasattr(args, 'get-active-codecs'):
        print(ctrl.getActiveCodecs(args.get-active-codec))

    if args.set-active-codecs:
        ctrl.setActiveCodecList(codec_list=args.set-active-codecs)

    if args.enable:
        ctrl.setAccountEnable(args.enable, True)

    if args.disable:
        ctrl.setAccountEnable(args.enable, False)

    if args.register:
        ctrl.setAccountRegistered(args.register, True)

    if args.unregister:
        ctrl.setAccountRegistered(args.unregister, False)

    if args.get-call-list:
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
        ctrl.Dtmf(args.dtmf)

    if args.test:
        DRingTester().start(ctrl, args.test)

    if args.toggleVideo:
        if not ctrl.videomanager:
            print("Error: daemon without video support")
            sys.exit(1)
        import time
        while True:
            time.sleep(2)
            ctrl.videomanager.startCamera()
            time.sleep(2)
            ctrl.videomanager.stopCamera()
