#!/usr/bin/env python
import signal

import time
import sys

import getopt
import gtk

from threading import Thread
from threading import Event

print "Import SFLphone"
from sflphonectrlsimple import SflPhoneCtrlSimple

# def killhandler(signum, frame):
#     raise IOError("Couldn't open device!")

# signal.signal(signal.SIGKILL, killhandler)

def acceptOnIncomingCall(sflphone):
    time.sleep(0.2)
    sflphone.Accept(sflphone.currentCallId)

def acceptOnIncomingCallHangup(sflphone):
    time.sleep(0.2)
    sflphone.Accept(sflphone.currentCallId)
    time.sleep(0.5)
    sflphone.HangUp(sflphone.currentCallId)

class SflPhoneTests():

    def __init__(self, sfl):
        print "Create test instance"
        self.sflphone = sfl

    def test_get_allaccounts_methods(self):

        for account in self.getAllAccounts():
            print "  " + account
        
        for account in self.getAllRegisteredAccounts():
            print "  " + account

        for account in self.getAllSipAccounts():
            print "  " + account

        for account in self.getAllIaxAccounts():
            print "  " + account

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


        accountID = self.sflphone.addAccount(accDetails)
        print "New Account ID " + accountID

        return accountID


    def test_remove_account(self, accountID):
        """Remove test account"""

        self.sflphone.removeAccount(accountID)
        print "Account with ID " + accountID + " removed"


    def test_ip2ip_send_hangup(self):
        """Make a call to a server (sipp) on port 5062"""
        i = 0
        while(i < 10):

            callid = self.sflphone.Call("sip:test@127.0.0.1:5062")
            time.sleep(0.5)
            
            self.sflphone.HangUp(callid)            
            time.sleep(0.5)

            i = i+1

        del self.sflphone


    def test_ip2ip_send_peer_hungup(self):
        """Make a call to a server (sipp) on port 5062"""
        i = 0
        while(i < 1):

            callid = self.sflphone.Call("sip:test@127.0.0.1:5062")
            time.sleep(1.0)

            i = i+1

        del self.sflphone


    def test_ip2ip_recv_hangup(self):
        """Wait for calls, answer then hangup"""

        # Add callback for this test
        self.sflphone.onIncomingCall_cb = acceptOnIncomingCallHangup

        # Start Glib mainloop
        self.sflphone.start()


    def test_ip2ip_recv_peer_hungup(self):
        """Wait for calls, answer, peer hangup"""
        # Add callback for this test
        self.sflphone.onIncomingCall_cb = acceptOnIncomingCall

        # Start Glib mainloop
        self.sflphone.start()


    def test_account_send_hangup(self):
        """Send new account call, hangup once peer answered"""

        i = 0
        while(i < 1):

            callid = self.sflphone.Call("27182")
            time.sleep(1.0)
            
            self.sflphone.HangUp(callid)            
            time.sleep(1.0)

            i = i+1

        # del self.sflphone


    def test_account_send_peer_hungup(self):
        """Send new account call, hangup once peer answered"""

        i = 0
        while(i < 10):

            callid = self.sflphone.Call("27182")
            time.sleep(1.0)

            i = i+1

        del self.sflphone


    def test_account_recv_hangup(self):
        """Register an account and wait for incoming calls"""

        # Add callback for this test
        self.sflphone.onIncomingCall_cb = acceptOnIncomingCallHangup

        # Start Glib mainloop
        self.sflphone.start()


    def test_account_recv_peer_hungup(self):
        """Register an account and wait for incoming calls"""

        # Add callback for this test
        self.sflphone.onIncomingCall_cb = acceptOnIncomingCall

        # Start Glib mainloop
        self.sflphone.start()


    def test_ip2ip_send_hold_offhold(self):
        """Send new call, hold this call, offhold, hangup"""
        i = 0
        while(i < 10):

            callid = self.sflphone.Call("sip:test@127.0.0.1:5062")
            time.sleep(0.5)

            self.sflphone.Hold(callid)
            time.sleep(0.5)

            self.sflphone.UnHold(callid)
            time.sleep(0.5)
            
            self.sflphone.HangUp(callid)            
            time.sleep(0.5)

            i = i+1

        del self.sflphone


    def test_account_send_transfer(self):
        """Send new calls, transfer it to a new instance"""

        i = 0
        while(i < 1):

            callid = self.sflphone.Call("27182")
            time.sleep(1.0)
            
            self.sflphone.Transfer(callid,"14142")
            # self.sflphone.HangUp(callid)            
            # time.sleep(1.0)

            i = i+1



# Open sflphone and connect to sflphoned through dbus
sflphone = SflPhoneCtrlSimple(True)

# Init test suite 
testsuite = SflPhoneTests(sflphone)

# Register the first account available, should be the test account
sflphone.setFirstRegisteredAccount();


# SCENARIO 1: IP2IP Normal flow calls

# Test 1: - Send an IP2IP call
#         - Hangup
# testsuite.test_ip2ip_send_hangup()

# Test 2: - Send an IP2IP call
#         - Peer Hangup
# testsuite.test_ip2ip_send_peer_hungup()

# Test 3: - Receive an IP2IP call
#         - Hangup
# testsuite.test_ip2ip_recv_hangup()

# Test 4: - Receive an IP2IP call
#         - Peer Hangup
# testsuite.test_ip2ip_recv_peer_hungup()



# SCENARIO 2: ACCOUNT Normal flow calls

# Test 1: - Send an ACCOUNT call
#         - Hangup
# testsuite.test_account_send_hangup()

# Test 2: - Send an ACCOUNT call
#         - Peer Hangup
# testsuite.test_account_send_peer_hungup()

# Test 3: - Receive an ACCOUNT call
#         - Hangup
# testsuite.test_account_recv_hangup()

# Test 4: - Receive an ACCOUNT call
#         - Peer Hangup
# testsuite.test_account_recv_peer_hungup()


# SCENARIO 3: IP2IP Call, HOLD/OFFHOLD

# Test 1: - Send an IP2IP call
#         - Put this call on HOLD
#         - Off HOLD this call
#         - Hangup
# testsuite.test_ip2ip_send_hold_offhold()


# SCENARIO 4: IP2IP Call, HOLD/OFFHOLD

# Test 1: - Send an IP2IP call
#         - Transfer this call to another sipp instance
#         - Hangup
testsuite.test_account_send_transfer()
