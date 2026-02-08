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

# Define remote IP address constant
REMOTEADDR_lo="127.0.0.1:5062"
REMOTEADDR_lo2="127.0.0.1:5064"
REMOTEADDR_lo3="127.0.0.1:5066"

# Defines phone numbers
PHONE1="27182"
PHONE2="31416"
PHONE3="14142"


# Define function callback to emulate UA behavior on
# receiving a call (peer end))
def acceptOnIncomingCall(sflphone):

    sflphone.Accept(sflphone.currentCallId)


# Define function callback to emulate UA behavior on
# receiving a call and hanging up
def acceptOnIncomingCallEnd(sflphone):

    sflphone.Accept(sflphone.currentCallId)
    sflphone.End(sflphone.currentCallId)


# Define function callback to emulate UA behavior on
# refusing a call
def refuseOnIncomingCall(sflphone):
    # time.sleep(0.5)
    sflphone.Decline(sflphone.currentCallId)


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


    # SCENARIO 1 Test 1
    def test_ip2ip_send_end(self):
        """Make a call to a server (sipp) on port 5062"""
        i = 0
        while(i < 500):

            callid = self.sflphone.Call("sip:test@" + REMOTEADDR_lo)
            time.sleep(0.5)

            self.sflphone.End(callid)
            time.sleep(0.5)

            i = i+1

        self.sflphone.unregister()
        del self.sflphone


    # SCENARIO 1 Test 2
    def test_ip2ip_send_peer_ended(self):
        """Make a call to a server (sipp) on port 5062"""
        i = 0
        while(i < 10):

            callid = self.sflphone.Call("sip:test@" + REMOTEADDR_lo)
            time.sleep(1.0)

            i = i+1

        del self.sflphone


    # SCENARIO 1 Test 3
    def test_ip2ip_recv_end(self):
        """Wait for calls, answer then end"""

        # Add callback for this test
        self.sflphone.onIncomingCall_cb = acceptOnIncomingCallEnd

        # Start Glib mainloop
        self.sflphone.start()




    # SCENARIO 1 Test 4
    def test_ip2ip_recv_peer_ended(self):
        """Wait for calls, answer, peer end"""

        # Add callback for this test
        self.sflphone.onIncomingCall_cb = acceptOnIncomingCall

        # Start Glib mainloop
        self.sflphone.start()


    # SCENARIO 2 Test 1
    def test_account_send_end(self):
        """Send new account call, end once peer answered"""

        i = 0
        while(i < 10):

            callid = self.sflphone.Call(PHONE1)
            time.sleep(0.2)

            self.sflphone.End(callid)
            time.sleep(0.2)

            i = i+1

        # del self.sflphone


    # SCENARIO 2 Test 2
    def test_account_send_peer_ended(self):
        """Send new account call, end once peer answered"""

        i = 0
        while(i < 10):

            callid = self.sflphone.Call(PHONE1)
            time.sleep(1.0)

            i = i+1

        del self.sflphone


    # SCENARIO 2 Test 3
    def test_account_recv_end(self):
        """Register an account and wait for incoming calls"""

        # Add callback for this test
        self.sflphone.onIncomingCall_cb = acceptOnIncomingCallEnd

        # Start Glib mainloop
        self.sflphone.start()


    # SCENARIO 2 Test 4
    def test_account_recv_peer_ended(self):
        """Register an account and wait for incoming calls"""

        # Add callback for this test
        self.sflphone.onIncomingCall_cb = acceptOnIncomingCall

        # Start Glib mainloop
        self.sflphone.start()


    # SCENARIO 3 Test 1
    def test_ip2ip_send_hold_offhold(self):
        """Start call, hold call, resume call, end call"""
        i = 0
        while(i < 10):

            callid = self.sflphone.Call("sip:test@" + REMOTEADDR_lo)
            time.sleep(0.5)

            self.sflphone.Hold(callid)
            time.sleep(0.5)

            self.sflphone.Resume(callid)
            time.sleep(0.5)

            self.sflphone.End(callid)
            time.sleep(0.5)

            i = i+1

        del self.sflphone


    # SCENARIO 4 Test 1
    def test_account_send_transfer(self):
        """Send new calls, transfer it to a new instance"""

        i = 0
        while(i < 1):

            callid = self.sflphone.Call(PHONE1)
            time.sleep(1.0)

            self.sflphone.Transfer(callid,PHONE3)
            # self.sflphone.End(callid)
            # time.sleep(1.0)

            i = i+1


    # SCENARIO 5 Test 1
    def test_ip2ip_recv_refuse(self):
        """Receive an incoming IP2IP call, decline it"""

        # Add callback for this test
        self.sflphone.onIncomingCall_cb = refuseOnIncomingCall

        # Start Glib mainloop
        self.sflphone.start()


    # SCENARIO 6 Test 1
    def test_mult_ip2ip_send_end(self):
        """Make a first call to a sipp server (5062) and a second to sipp server (5064)"""
        i = 0
        while(i < 500):

            callid1 = self.sflphone.Call("sip:test@" + REMOTEADDR_lo)
            time.sleep(0.1)

            callid2 = self.sflphone.Call("sip:test@" + REMOTEADDR_lo2)
            time.sleep(0.1)

            callid3 = self.sflphone.Call("sip:test@" + REMOTEADDR_lo3)
            time.sleep(0.1)

            self.sflphone.End(callid1)
            time.sleep(0.1)

            self.sflphone.End(callid2)
            time.sleep(0.1)

            self.sflphone.End(callid3)
            time.sleep(0.1)

            i = i+1

        del self.sflphone


    # SCENARIO 6 Test 2
    def test_mult_ip2ip_send_end(self):
        """Receive multiple calls peer end"""

        # Add callback for this test
        self.sflphone.onIncomingCall_cb = acceptOnIncomingCall

        # Start Glib mainloop
        self.sflphone.start()

        del self.sflphone



# Open sflphone and connect to sflphoned through dbus
sflphone = SflPhoneCtrlSimple(True)

# Init test suite
testsuite = SflPhoneTests(sflphone)

# Register the first account available, should be the test account
sflphone.setFirstRegisteredAccount();


# ============================ Test Suite ============================



# SCENARIO 1: IP2IP Normal flow calls

# Test 1: - Send an IP2IP call
#         - End
# testsuite.test_ip2ip_send_end()

# Test 2: - Send an IP2IP call
#         - Peer End
# testsuite.test_ip2ip_send_peer_ended()

# Test 3: - Receive an IP2IP call
#         - End
testsuite.test_ip2ip_recv_end()

# Test 4: - Receive an IP2IP call
#         - Peer End
# testsuite.test_ip2ip_recv_peer_ended()



# SCENARIO 2: ACCOUNT Normal flow calls

# Test 1: - Send an ACCOUNT call
#         - End
# testsuite.test_account_send_end()

# Test 2: - Send an ACCOUNT call
#         - Peer End
# testsuite.test_account_send_peer_ended()

# Test 3: - Receive an ACCOUNT call
#         - End
# testsuite.test_account_recv_end()

# Test 4: - Receive an ACCOUNT call
#         - Peer End
# testsuite.test_account_recv_peer_ended()



# SCENARIO 3: IP2IP Call, HOLD/OFFHOLD

# Test 1: - Send an IP2IP call
#         - Put this call on HOLD
#         - Off HOLD this call
#         - End
# testsuite.test_ip2ip_send_hold_offhold()



# SCENARIO 4: IP2IP Call, HOLD/OFFHOLD

# Test 1: - Send an IP2IP call
#         - Transfer this call to another sipp instance
#         - End
# testsuite.test_account_send_transfer()



# SCENARIO 5: IP2IP Call, Decline

# Test 1: - Receive an incoming call
#         - End without answer
# testsuite.test_ip2ip_recv_refuse()



# SCENARIO 6: Multiple simultaneous calls

# Test 1: - Send multiple simultaneous IP2IP call
#         - End
# testsuite.test_mult_ip2ip_send_end()

# Test 2: - Receive simultaneous IP2IP call
#         - End
# testsuite.test_mult_ip2ip_send_end()
