#!/usr/bin/env python
import time
import sys

import getopt

from sflphonectrlsimple import SflPhoneCtrlSimple


class SflPhoneTests(SflPhoneCtrlSimple):


    def test_get_allaccounts_methods(self):

        for account in self.getAllAccounts():
            print "  " + account
        
        for account in self.getAllRegisteredAccounts():
            print "  " + account

        for account in self.getAllSipAccounts():
            print "  " + account

        for account in self.getAllIaxAccounts():
            print "  " + account

    def test_make_iptoip_call(self):
        """Make a call to a server (sipp) on port 5062"""
        i = 0
        while(i < 10):

            callid = self.Call("sip:test@127.0.0.1:5062")
            time.sleep(0.4)
            
            self.HangUp(callid)            
            time.sleep(0.4)

            i = i+1

    def test_make_account_call(self):
        """Register an account on a remote server and make several calls"""

        self.setAccount("Account:1258495784");
        time.sleep(3)

        i = 0
        while(i < 50):

            callid = self.Call("5000")
            time.sleep(0.4)

            self.HangUp(callid)
            time.sleep(0.4)

            i = i+1


    def test_create_account(self):
        """Create a new sip account"""

        CONFIG_ACCOUNT_TYPE = "Account.type"  
	CONFIG_ACCOUNT_ALIAS = "Account.alias"
	HOSTNAME = "hostname"
	USERNAME = "username"
	PASSWORD = "password"
	
        accDetails = {CONFIG_ACCOUNT_TYPE:"SIP", CONFIG_ACCOUNT_ALIAS:"testsuiteaccount",
                      HOSTNAME:"192.168.50.79", USERNAME:"31416",
                      PASSWORD:"1234"}


        accountID = self.addAccount(accDetails)
        print "New Account ID " + accountID

        return accountID


    def test_remove_account(self, accountID):
        """Remove test account"""

        self.removeAccount(accountID)
        print "Account with ID " + accountID + " removed"


# Open sflphone and connect to sflphoned through dbus 
sflphone = SflPhoneTests()

# Test 1: Makke approximately one IP2IP call per second 
# to a sipp uas on local addrress
#sflphone.test_make_iptoip_call()


# Test 2: 
accountID = sflphone.test_create_account()
# sflphone.test_make_account_call()
time.sleep(0.3)
# sflphone.test_remove_account(accountID)
