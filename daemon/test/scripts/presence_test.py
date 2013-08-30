#!/usr/bin/python
# -*- coding: utf-8 -*-
print "\
#                               --SFLPhone--                                #\n\
#                                                                           #\n\
# copyright:   Savoir-Faire Linux (2013)                                    #\n\
# author:      Patrick keroulas <patrick.keroulas@savoirfairelinux.com>     #\n\
# description: This script sends a sequence of methods to the daemon        #\n\
#              through the dbus to test the presence feature of SFLPhone.   #\n\
#              SET THE PARAMS IN THE 'data' SECTION OF THIS SCRIPT BEFORE   #\n\
#              YOU EXECUTE IT. 'The normal mode process the tasks           #\n\
#              in order while the random mode generates a random sequence   #\n\
#              A Freeswitch server must be setup since it supports PUBLISH  #\n\
#              requests and Asterisk doesn't.                               #\n\
#              The user must have a valid account. This script will         #\n\
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
callManagerBus          = bus.get_object('org.sflphone.SFLphone', '/org/sflphone/SFLphone/CallManager')
callManager             = dbus.Interface(callManagerBus, dbus_interface='org.sflphone.SFLphone.CallManager')
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

def registerSend(arg):
    configurationManager.sendRegister(arg['acc'],arg['enable'])
    logging.info('Send register : '+ str(arg))

#-------------------------   Presence functions      ----------------------------



def presSubscribe(arg):
    callManager.subscribePresSubClient(arg['acc'],arg['buddy'],arg['flag'])
    logging.info('Subscribe to ' + str(arg))

def presSend(arg):
    callManager.sendPresence(arg['acc'],arg['status'],arg['note'])
    logging.info('Send to ' + str(arg))

def presSubApprove(arg):
    callManager.approvePresSubServer(arg['uri'],arg['flag'])
    logging.info('Approve subscription from' + str(arg))

def newPresSubCientNotificationHandler(uri, status, activity):
    global subscribe_flag
    if subscribe_flag:
        logging.info("Received DBus signal : < from:"+str(uri)+" (status:" + str(status)+ ", "+ str(activity)+ ").")
    else:
        logging.error("Not supposed to receive DBus singal when unsubscribe.")

def newPresSubServerRequestHandler(uri):
    logging.info("Received a PresenceSubscription request from " +str(uri))
    subscriber_uri = uri

def randbool():
    return bool(randint(0,1))

#---------------------------     Data           -------------------------------

TEST_DURATION = 10 # in sec
first_account = get_first_account()
IP2IP = 'IP2IP'
server_ip = '192.168.50.124'

host_user = '1001'
host_ip = '192.168.50.196'
host_uri = '<sip:'+host_user+'@'+server_ip+'>'

# self subscribing
buddy_user = host_user
buddy_ip = host_ip
buddy_uri = '<sip:'+buddy_user+'@'+server_ip+'>'
buddy_ip_uri = '<sip:'+buddy_ip+'>' # IP2IP
subscriber_uri = ''

#----------------------------    Sequence        ----------------------------

start_time = 0
task_count = 0
task_N = 0

SEQ_MODE_NORMAL = 0
SEQ_MODE_RANDOM = 1
sequence_mode = SEQ_MODE_NORMAL

task_list = [

    # regular account
    (presSubscribe, {'acc':first_account,'buddy':buddy_uri,'flag':True}),
    (presSend, {'acc':first_account,'status':randbool,'note':'Oh yeah!'}),
    (presSubscribe, {'acc':first_account,'buddy':buddy_uri,'flag':False}),
    (presSend, {'acc':first_account,'status':randbool,'note':'This notify should not be recieved'}),

    # IP2IP
    (presSubscribe, {'acc': IP2IP,'buddy':buddy_ip_uri,'flag':True}),
    (presSend, {'acc': IP2IP,'status':False,'note':'Wait for approval'}),
    (presSubApprove, {'uri':subscriber_uri,'flag':randbool}),
    (presSend, {'acc': IP2IP,'status':randbool(),'note':'Oh yeah!'}),
    (presSubscribe, {'acc': IP2IP,'buddy':buddy_ip_uri,'flag':False}),
]


def run():

    if sequence_mode == SEQ_MODE_NORMAL:
        global task_count
        task_index = task_count%task_N
        task_count += 1
    elif sequence_mode == SEQ_MODE_RANDOM:
        task_index = randint(0,task_N-1)

    task_list[task_index][0](task_list[task_index][1])

    if(int(time.time()-start_time) < TEST_DURATION):
        gobject.timeout_add(randint(50,2000), run) # random time step in ms
    else:
        logging.info("Test sequence finished")
        # TODO clear dbus sessio. Unfortunately stackoverflow.com is down today


if __name__ == '__main__':

    try:
        # dbus signal monitor
        callManagerBus.connect_to_signal("newPresSubCientNotification", newPresSubCientNotificationHandler, dbus_interface='org.sflphone.SFLphone.CallManager')
        callManagerBus.connect_to_signal("newPresSubServerRequest", newPresSubServerRequestHandler, dbus_interface='org.sflphone.SFLphone.CallManager')

        registerSend({'acc':first_account, 'enable':True})
        start_time = time.time()
        task_N = len(task_list)
        #sequence_mode = SEQ_MODE_RANDOM

        run()

    except Exception as e:
        print e

    loop = gobject.MainLoop()
    loop.run()
