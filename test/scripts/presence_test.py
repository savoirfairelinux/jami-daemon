#!/usr/bin/python
# -*- coding: utf-8 -*-
print "\
#                               --SFLPhone--                                #\n\
#                                                                           #\n\
# copyright:   Savoir-Faire Linux (2013)                                    #\n\
# author:      Patrick keroulas <patrick.keroulas@savoirfairelinux.com>     #\n\
# description: This script sends a sequence of methods to the daemon        #\n\
#              through the dbus to test the presence feature of SFLPhone.   #\n\
#              SET THE PARAMS IS THE 'data' SECTION OF THIS SCRIPT BEFORE   #\n\
#              YOU EXECUTE IT. 'The normal mode process the tasks           #\n\
#              in order while the random mode generates a random sequence   #\n\
#              A Freeswitch server must be setup since it supports PUBLISH  #\n\
#              requests and Asterisk doesn't.                               #\n\
#              The user must have 2 valid accounts. This script will        #\n\
#              use the first account in the list and the IP2IP account.     #\n\
#              This is a self subscribe test, set another buddy IP if needed#\n\
#\n"



import time, sys, gobject
from random import randint
import dbus, dbus.mainloop.glib
import logging, commands


#-----------------     logger to file and stdout  ------------------------------
f = '%(asctime)s - %(name)s - %(levelname)s - %(message)s'
logfile = 'sflphone_doombot.log'
logging.basicConfig(filename=logfile,level=logging.INFO,format=f) # log to file
logger = logging.getLogger()
ch = logging.StreamHandler(sys.stdout)  #log to console
ch.setLevel(logging.INFO)
formatter = logging.Formatter(f)
ch.setFormatter(formatter)
logger.addHandler(ch)

#------------------      Initialise DBUS          ------------------------------
dbus.mainloop.glib.DBusGMainLoop(set_as_default=True)
bus = dbus.SessionBus()
presenceManagerBus          = bus.get_object('org.sflphone.SFLphone', '/org/sflphone/SFLphone/PresenceManager')
presenceManager             = dbus.Interface(presenceManagerBus, dbus_interface='org.sflphone.SFLphone.PresenceManager')
configurationManagerBus = bus.get_object('org.sflphone.SFLphone', '/org/sflphone/SFLphone/ConfigurationManager')
configurationManager    = dbus.Interface(configurationManagerBus, dbus_interface='org.sflphone.SFLphone.ConfigurationManager')


#-------------------------    General purpose functions     ----------------------------

#Get the first non-IP2IP account
def get_first_account():
    accounts = configurationManager.getAccountList()
    for i, v in enumerate(accounts):
        if v != "IP2IP":
            details = configurationManager.getAccountDetails(v)
            if details["Account.type"] == True or details["Account.type"] == "SIP":
                return v
    return "IP2IP"

def get_account_list():
    accounts = configurationManager.getAccountList()
    result = []
    for i, v in enumerate(accounts):
        if v != "IP2IP":
            details = configurationManager.getAccountDetails(v)
            if details["Account.type"] == True or details["Account.type"] == "SIP":
                result.append(v)
    return result

def registerSend(arg):
    configurationManager.sendRegister(arg['acc'],arg['enable'])
    logging.info('> REGISTER : '+ str(arg))

#-------------------------   Presence functions      ----------------------------

def presSubscribe(arg):
    presenceManager.subscribeBuddy(arg['acc'],arg['buddy'],arg['flag'])
    logging.info('> SUBSCRIBE to ' + str(arg))

def presSend(arg):
    presenceManager.publish(arg['acc'],arg['status'],arg['note'])
    logging.info('> PUBLISH ' + str(arg))

def presSubApprove(arg):
    presenceManager.answerServerRequest(arg['uri'],arg['flag'])
    logging.info('> APPROVE subscription from' + str(arg))

def newPresSubCientNotificationHandler(acc, uri, status, activity):
    logging.info("< SIGNAL : Notification for  acc:"+str(acc)+", from:"+str(uri)+" (status:" + str(status)+ ", "+ str(activity)+ ").")

def newPresSubServerRequestHandler(uri):
    logging.info("< SIGNAL : PresenceSubscription request from " +str(uri))
    subscriber_uri = uri

def subcriptionStateChangedHandler(acc,uri,flag):
    logging.info("< SIGNAL : new subscriptionState request for acc:"+str(acc)+" uri " +str(uri) + " flag:" + str(flag))

def serverErrorHandler(acc, error, msg):
    logging.info("< SIGNAL : error from server:"+str(error)+" . "+str(msg))

def randbool():
    return bool(randint(0,1))

#---------------------------     Data           -------------------------------

TEST_DURATION = 30 # in sec
acc_1 = get_account_list()[0]
print 'acc_1 : ' + str(acc_1)
acc_2 = get_account_list()[1]
print 'acc_2 : ' + str(acc_2)
IP2IP = 'IP2IP'
server_ip = '192.95.9.63' # asterisk test server

host_user = '6001'
host_ip = '192.168.50.196'
host_uri = '<sip:'+host_user+'@'+server_ip+'>'

buddy_uri_1 = '<sip:6001@'+server_ip+'>'
buddy_uri_2 = '<sip:6002@'+server_ip+'>'
buddy_ip = host_ip # self subscribing
buddy_ip_uri = '<sip:'+buddy_ip+'>' # IP2IP
subscriber_uri = ''

#----------------------------    Sequence        ----------------------------

start_time = 0
task_count = 0
task_N = 0

SEQ_MODE_NORMAL = 0
SEQ_MODE_RANDOM = 1
sequence_mode = SEQ_MODE_NORMAL


# regular test
task_list = [

    (registerSend,{'acc':acc_1, 'enable':True}),
    (presSubscribe, {'acc':acc_1,'buddy':buddy_uri_1,'flag':True}),

    (registerSend,{'acc':acc_2, 'enable':True}),
    (presSubscribe, {'acc':acc_2,'buddy':buddy_uri_2,'flag':True}),

    (presSend, {'acc':acc_2,'status':randbool(),'note':buddy_uri_1+'is here!'}),
    (presSend, {'acc':acc_1,'status':randbool(),'note':buddy_uri_2+'is here'}),

    (presSubscribe, {'acc':acc_1,'buddy':'<sip:6003@192.95.9.63>','flag':True}),

    (registerSend,{'acc':acc_1, 'enable':False}),
    (presSubscribe, {'acc':acc_2,'buddy':buddy_uri_2,'flag':False}),

    (registerSend,{'acc':acc_2, 'enable':False}),
    (presSubscribe, {'acc':acc_1,'buddy':buddy_uri_1,'flag':False}),
]


"""
# simple sub
task_list = [

    (presSubscribe, {'acc':acc_2,'buddy':buddy_uri_2,'flag':True}),
    (presSubscribe, {'acc':acc_2,'buddy':'<sip:6003@192.95.9.63>','flag':True}),
]
"""

"""
# IP2IP
task_list = [
    (presSubscribe, {'acc': IP2IP,'buddy':buddy_ip_uri,'flag':True}),
    (presSend, {'acc': IP2IP,'status':randbool(),'note':'This notify should not be recieved'}),
    (presSubApprove, {'uri':subscriber_uri,'flag':randbool()}),
    (presSend, {'acc': IP2IP,'status':randbool(),'note':'Oh yeah!'}),
    (presSubscribe, {'acc': IP2IP,'buddy':buddy_ip_uri,'flag':False}),
]
"""


def run():

    if sequence_mode == SEQ_MODE_NORMAL:
        global task_count
        task_index = task_count%task_N
        task_count += 1
        if task_count == task_N+1: # one loop
            return

    elif sequence_mode == SEQ_MODE_RANDOM:
        task_index = randint(0,task_N-1)

    task_list[task_index][0](task_list[task_index][1])

    if(int(time.time()-start_time) < TEST_DURATION):
        gobject.timeout_add(2000, run) # const time step in ms
        #gobject.timeout_add(randint(50,2000), run) # random time step in ms
    else:
        logging.info("Test sequence finished")
        # TODO clear dbus sessio. Unfortunately stackoverflow.com is down today


if __name__ == '__main__':

    try:
        # dbus signal monitor
        presenceManagerBus.connect_to_signal("newBuddyNotification", newPresSubCientNotificationHandler, dbus_interface='org.sflphone.SFLphone.PresenceManager')
        presenceManagerBus.connect_to_signal("newServerSubscriptionRequest", newPresSubServerRequestHandler, dbus_interface='org.sflphone.SFLphone.PresenceManager')
        presenceManagerBus.connect_to_signal("subcriptionStateChanged", subcriptionStateChangedHandler, dbus_interface='org.sflphone.SFLphone.PresenceManager')
        presenceManagerBus.connect_to_signal("serverError", serverErrorHandler, dbus_interface='org.sflphone.SFLphone.PresenceManager')

        start_time = time.time()
        task_N = len(task_list)
        #sequence_mode = SEQ_MODE_RANDOM

        run()

    except Exception as e:
        print e

    loop = gobject.MainLoop()
    loop.run()
