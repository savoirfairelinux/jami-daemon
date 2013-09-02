#!/usr/bin/python
# -*- coding: utf-8 -*-
print "\
#                               --SFLPhone--                                 #\n\
#        /¯¯¯¯\  /¯¯¯¯\ /¯¯¯¯\  /¯¯¯\_/¯¯¯\   /¯¯¯¯¯\  /¯¯¯¯\ |¯¯¯¯¯¯¯|      #\n\
#       / /¯\ | /  /\  \  /\  \| /¯\  /¯\  |  | |¯| | |  /\  | ¯¯| |¯¯       #\n\
#      / /  / |/  | |  | | |  || |  | |  | |  |  ¯ <  |  | | |   | |         #\n\
#     / /__/ / |   ¯   |  ¯   || |  | |  | |  | |¯| | |  |_| |   | |         #\n\
#    |______/   \_____/ \____/ |_|  |_|  |_|  \_____/  \____/    |_|         #\n\
#                                                                            #\n\
# copyright:   Savoir-Faire Linux (2012)                                     #\n\
# author:      Emmanuel Lepage Vallee <emmanuel.lepage@savoirfairelinux.com> #\n\
# description: This script perform stress tests to trigger rare race         #\n\
#               conditions or ASSERT caused by excessive load. This script   #\n\
#               should, in theory, never crash or end the sflphone daemon    #\n"

import dbus
import time
import sys
from random import randint

#Initialise DBUS
bus = dbus.SessionBus()
callManagerBus          = bus.get_object('org.sflphone.SFLphone', '/org/sflphone/SFLphone/CallManager')
callManager             = dbus.Interface(callManagerBus, dbus_interface='org.sflphone.SFLphone.CallManager')
configurationManagerBus = bus.get_object('org.sflphone.SFLphone', '/org/sflphone/SFLphone/ConfigurationManager')
configurationManager    = dbus.Interface(configurationManagerBus, dbus_interface='org.sflphone.SFLphone.ConfigurationManager')

#---------------------------------------------------------------------#
#                                                                     #
#                                Tools                                #
#                                                                     #
#---------------------------------------------------------------------#

#Get the first non-IP2IP account
def get_first_account():
	accounts = configurationManager->getAccountList()
	for i, v in enumerate(accounts):
		if v != "IP2IP":
			details = configurationManager->getAccountDetails(v)
			if details["Account.type"] == True or details["Account.type"] == "SIP":
				return v
	return "IP2IP"

#Get the first IAX account
def get_first_iax_account():
	accounts = configurationManager->getAccountList()
	for i, v in enumerate(accounts):
		if v != "IP2IP":
			details = configurationManager->getAccountDetails(v)
			if details["Account.type"] != True and details["Account.type"] != "SIP":
				return v
	return "IP2IP"

def get_account_number(account):
	details = configurationManager->getAccountDetails(account)
	return details["Account.username"]

def answer_all_calls():
	calls = callManager.getCallList()
	for i, v in enumerate(calls):
		details = callManager.getCallDetails(v)
		if details["CALL_STATE"] == "INCOMING":
			callManager.accept(v)

#Return true is the account is registered
def check_account_state(account):
	details = configurationManager->getAccountDetails(account)
	#details = {'test':1,'test2':2,'registrationStatus':3}
	return details['Account.registrationStatus'] == "REGISTERED"

#Meta test, common for all tests
def meta_test(test_func):
	try:
		for y in range(0,15):
			for x in range(0,10):
				ret = test_func()
				if ret['code'] > 0:
					sys.stdout.write(' \033[91m(Failure)\033[0m\n')
					print "      \033[0;33m"+ret['error']+"\033[0m"
					return 1
			sys.stdout.write('#')
			sys.stdout.flush()
		sys.stdout.write(' \033[92m(Success)\033[0m\n')
	except dbus.exceptions.DBusException:
		sys.stdout.write(' \033[91m(Failure)\033[0m\n')
		print "      \033[0;33mUnit test \"stress_answer_hangup_server\" failed: Timeout, the daemon is unreachable, it may be a crash, a lock or an assert\033[0m"
		return 1
	#except Exception:
		#sys.stdout.write(' \033[91m(Failure)\033[0m\n')
		#print "      \033[0;33mUnit test \"stress_answer_hangup_server\" failed: Unknown error, disable 'except Exception' for details\033[0m"
		#return 1
	return 0

#Add a new test
suits = {}
def add_to_suit(test_suite_name,test_name,test_func):
	if not test_suite_name in suits:
		suits[test_suite_name] = []
	suits[test_suite_name].append({'test_name':test_name,'test_func':test_func})

# Run tests
def run():
	counter = 1
	results = {}
	for k in suits.keys():
		print "\n\033[1mExecuting \""+str(k)+"\" tests suit:\033[0m ("+str(counter)+"/"+str(len(suits))+")"
		for  i, v in enumerate(suits[k]):
			sys.stdout.write("   ["+str(i+1)+"/"+str(len(suits[k]))+"] Testing \""+v['test_name']+"\": ")
			sys.stdout.flush()
			retval = meta_test(v['test_func'])
			if not k in results:
				results[k] = 0
			if retval > 0:
				results[k]= results[k] + 1
		counter = counter + 1

	print "\n\n\033[1mSummary:\033[0m"
	totaltests = 0
	totalsuccess = 0
	for k in suits.keys():
		print "   Suit \""+k+"\": "+str(len(suits[k])-results[k])+"/"+str(len(suits[k]))
		totaltests = totaltests + len(suits[k])
		totalsuccess = totalsuccess + len(suits[k])-results[k]

	print "\nTotal: "+str(totalsuccess)+"/"+str(totaltests)+", "+str(totaltests-totalsuccess)+" failures"


#---------------------------------------------------------------------#
#                                                                     #
#                              Variables                              #
#                                                                     #
#---------------------------------------------------------------------#
first_account = get_first_account()
first_iax_account = get_first_iax_account()
first_account_number = get_account_number(first_account)


#---------------------------------------------------------------------#
#                                                                     #
#                             Unit Tests                              #
#                                                                     #
#---------------------------------------------------------------------#

# This unit case test the basic senario of calling asterisk/freeswitch and then hanging up
# It call itself to make the test simpler, this also test answering up as a side bonus
def stress_answer_hangup_server():
	callManager.placeCall(first_account,str(randint(100000000,100000000000)),first_account_number)
	time.sleep(0.05)
	calls = callManager.getCallList()

	# Check if the call worked
	if len(calls) < 2:
		if not check_account_state(first_account):
			#TODO Try to register again instead of failing
			return {'code':2,'error':"Unit test \"stress_answer_hangup_server\" failed: Account went unregistered"}
		else:
			return {'code':1,'error':"Unit test \"stress_answer_hangup_server\" failed: Error while placing call, there is "+str(len(calls))+" calls"}
	else:

		#Accept the calls
		for i, v in enumerate(calls):
			time.sleep(0.05)
			#callManager.accept(v)

		#Hang up
		callManager.hangUp(calls[0])
	return {'code':0,'error':""}
add_to_suit("Place call",'Place, answer and hangup',stress_answer_hangup_server)



# This test is similar to stress_answer_hangup_server, but test using IP2IP calls
def stress_answer_hangup_IP2IP():
	callManager.placeCall(first_account,str(randint(100000000,100000000000)),"sip:127.0.0.1")
	time.sleep(0.05)
	calls = callManager.getCallList()

	# Check if the call worked
	if len(calls) < 2:
		if not check_account_state(first_account):
			#TODO Try to register again instead of failing
			return {'code':2,'error':"\nUnit test \"stress_answer_hangup_server\" failed: Account went unregistered"}
		else:
			return {'code':1,'error':"\nUnit test \"stress_answer_hangup_server\" failed: Error while placing call, there is "+str(len(calls))+" calls"}
	else:
		#Accept the calls
		for i, v in enumerate(calls):
			time.sleep(0.05)
			#callManager.accept(v)
		#Hang up
		callManager.hangUp(calls[0])
	return {'code':0,'error':""}
add_to_suit("Place call",'Place, answer and hangup (IP2IP)',stress_answer_hangup_IP2IP)

# Test various type of transfers between various type of calls
# Use both localhost and
def stress_transfers():
	for i in range(0,50): #repeat the tests
		for j in range(0,3): # alternate between IP2IP, SIP and IAX
			for k in range(0,2): #alternate between classic transfer and attended one
				acc1 = ""
				if j == 0:
					acc1 = first_account
				elif j == 1:
					acc1 = "IP2IP"
				else:
					acc1 = first_iax_account
				acc2 = ""
				if i%3 == 0: #Use the first loop to shuffle second account type
					acc2 = first_account
				elif i%3 == 1:
					acc2 = "IP2IP"
				else:
					acc2 = first_iax_account
				print "ACC1"+acc1+" ACC2 "+acc2+ " FIRST IAX "+ first_iax_account +" FISRT "+first_account
				destination_number = ""
				if acc2 == "IP2IP":
					destination_number = "sip:127.0.0.1"
				else:
					destination_number = configurationManager->getAccountDetails(acc2)["Account.username"]
				callManager.placeCall(acc1,str(randint(100000000,100000000000)),destination_number)
				second_call = None
				if k == 1:
					callManager.placeCall(acc1,str(randint(100000000,100000000000)),"188")
				answer_all_calls()

				if k == 1:
					first_call = None
					calls = callManager.getCallList()
					for i, v in enumerate(calls):
						if first_call == None:
							first_call = v
						else:
							callManager.attendedTransfer(v,first_call)
				else:
					calls = callManager.getCallList()
					for i, v in enumerate(calls):
						callManager.transfer(v,destination_number)


# This test make as tons or calls, then hangup them all as fast as it can
def stress_concurent_calls():
	for i in range(0,50): #repeat the tests
		for j in range(0,3): # alternate between IP2IP, SIP and IAX
			for k in range(0,2): #alternate between classic transfer and attended one
				acc1 = ""
				if j == 0:
					acc1 = first_account
				elif j == 1:
					acc1 = "IP2IP"
				else:
					acc1 = first_iax_account
				acc2 = ""
				if i%3 == 0: #Use the first loop to shuffle second account type
					acc2 = first_account
				elif i%3 == 1:
					acc2 = "IP2IP"
				else:
					acc2 = first_iax_account
				print "ACC1"+acc1+" ACC2 "+acc2+ " FIRST IAX "+ first_iax_account +" FISRT "+first_account
				destination_number = ""
				if acc2 == "IP2IP":
					destination_number = "sip:127.0.0.1"
				else:
					destination_number = configurationManager->getAccountDetails(acc2)["Account.username"]
				callManager.placeCall(acc1,str(randint(100000000,100000000000)),destination_number)
	calls = callManager.getCallList()
	for i, v in enumerate(calls):
		callManager.hangUp(v)
add_to_suit("Place call",'Many simultanious calls (IP2IP)',stress_concurent_calls)

# Test if SFLPhone can handle more than 50 simultanious IP2IP call over localhost
# Using localhost to save bandwidth, this is about concurent calls, not network load
#def stress_concurent_calls():

	## Create 50 calls
	#for i in range(0,50):
		#callManager.placeCall(first_account,str(randint(100000000,100000000000)),"sip:127.0.0.1")

	##TODO check if the could is right

	## Accept all calls that worked
	#calls = callManager.getCallList()
	#for i, v in enumerate(calls):
		#callManager.accept(v)

	## Hang up all calls
	#for i, v in enumerate(calls):
		#callManager.hangUp(v)
	#return {'code':0,'error':""}
#add_to_suit("Place call",'Many simultanious calls (IP2IP)',stress_concurent_calls)


# Test if a call can be put and removed from hold multiple time
def stress_hold_unhold_server():
	# Hang up everything left
	calls = callManager.getCallList()
	for i, v in enumerate(calls):
		callManager.hangUp(v)

	#Place a call
	callManager.placeCall(first_account,str(randint(100000000,100000000000)),first_account_number)
	calls = callManager.getCallList()
	if len(calls) < 1:
		return {'code':5,'error':"\nUnit test \"stress_hold_unhold\" failed: The call is gone"}
	call = calls[0]

	#Hold and unhold it
	for i in range(0,10):
		callManager.hold(call)
		details = callManager.getCallDetails(call)
		if not 'CALL_STATE' in details:
			return {'code':1,'error':"\nUnit test \"stress_hold_unhold\" failed: The call is gone (hold)"}
		if not details['CALL_STATE'] == "HOLD":
			return {'code':2,'error':"\nUnit test \"stress_hold_unhold\" failed: The call should be on hold, but is "+details['CALL_STATE']}
		callManager.unhold(call)
		details = callManager.getCallDetails(call)
		if not 'CALL_STATE' in details:
			return {'code':3,'error':"\nUnit test \"stress_hold_unhold\" failed: The call is gone (unhold)"}
		if not details['CALL_STATE'] == "CURRENT":
			return {'code':4,'error':"\nUnit test \"stress_hold_unhold\" failed: The call should be current, but is "+details['CALL_STATE']}
	return {'code':0,'error':""}
#add_to_suit("Hold call",'Hold and unhold',stress_hold_unhold_server)

#Run the tests
#run()
stress_transfers()
#kate: space-indent off; tab-indents  on; mixedindent off; indent-width 4;tab-width 4;
