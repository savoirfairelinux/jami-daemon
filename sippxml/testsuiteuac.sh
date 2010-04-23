#!/bin/bash


LOCALPORT=5062
LOCALIP_lo=127.0.0.1
LOCALIP_eth0=192.168.50.182

REMOTEADDR_lo=127.0.0.1:5060
REMOTEADDR_ast=192.168.50.79

# SCENARIO 1 Test 1
function test_ip2ip_send_hangup {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # start sipp server to receive calls from sflphone
    sipp -sf ip2ip_uas_recv_peer_hungup.xml ${REMOTEADDR_lo} -i ${LOCALIP_lo} -p ${LOCALPORT}

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # run python client and script to make calls
    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # kill every one
    # bashtrap
}

# SCENARIO 1 Test 2
function test_ip2ip_send_peer_hungup {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # start sipp server to receive calls from sflphone and then hangup
    sipp -sf ip2ip_uas_recv_hangup.xml ${REMOTEADDR_lo} -s ${REMOTEADDR_lo} -i ${LOCALIP_lo} -p ${LOCALPORT}

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # run python client and script to make calls
    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # kill every one
    bashtrap 
}


# SCENARIO 1 Test 3
function test_ip2ip_recv_hangup {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # wait some time to make sure client is bound
    # sleep 1;

    # start sipp client and send calls 
    sipp -sf ip2ip_uac_send_peer_hungup.xml ${REMOTEADDR_lo} -i ${LOCALIP_lo} -p ${LOCALPORT} -l 1 -m 10

    # kill every one
    # bashtrap 
}


# SCENARIO 1 Test 4
function test_ip2ip_recv_peer_hungup {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # wait some time to make sure client is bound
    # sleep 1;

    # start sipp client and send calls 
    sipp -sf ip2ip_uac_send_hangup.xml ${REMOTEADDR_lo} -i ${LOCALIP_lo} -p ${LOCALPORT} -l 1 -m 10

    # kill every one
    # bashtrap 
}


# SCENARIO 2 Test 1
function test_account_send_hangup {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # wait some time to make sure client is bound
    # sleep 1;

    # process only one registration
    sipp -sf account_uas_register.xml ${REMOTEADDR_ast} -i ${LOCALIP_eth0} -p ${LOCALPORT} -l 1 -m 1

    # start sipp client and send calls 
    sipp -sf account_uas_recv_peer_hungup.xml ${REMOTEADDR_ast} -i ${LOCALIP_eth0} -p ${LOCALPORT} -l 1

    # kill every one
    # bashtrap
}

# SCENARIO 2 Test 2
function test_account_send_peer_hungup {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # wait some time to make sure client is bound
    # sleep 1;

    # process only one registration
    sipp -sf account_uas_register.xml ${REMOTEADDR_ast} -i ${LOCALIP_eth0} -p ${LOCALPORT} -l 1 -m 1

    # start sipp client and send calls 
    sipp -sf account_uas_recv_hangup.xml ${REMOTEADDR_ast} -i ${LOCALIP_eth0} -p ${LOCALPORT} -l 1

    # kill every one
    # bashtrap
}

# SCENARIO 2 Test 3
function test_account_recv_hangup {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # wait some time to make sure client is bound
    # sleep 1;

    # process only one registration
    sipp -sf account_uas_register.xml ${REMOTEADDR_ast} -i ${LOCALIP_eth0} -p ${LOCALPORT} -l 1 -m 1

    # start sipp client and send calls 
    sipp -sf account_uac_send_peer_hungup.xml ${REMOTEADDR_ast} -i ${LOCALIP_eth0} -p ${LOCALPORT} -l 1 -m 10
    # kill every one
    # bashtrap
}

# SCENARIO 2 Test 4
function test_account_recv_peer_hungup {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # wait some time to make sure client is bound
    # sleep 1;

    # process only one registration
    sipp -sf account_uas_register.xml ${REMOTEADDR_ast} -i ${LOCALIP_eth0} -p ${LOCALPORT} -l 1 -m 1

    # start sipp client and send calls 
    sipp -sf account_uac_send_hangup.xml ${REMOTEADDR_ast} -i ${LOCALIP_eth0} -p ${LOCALPORT} -l 1 -m 10

    # kill every one
    # bashtrap
}

# SCENARIO 3 Test 1
function test_ip2ip_send_hold_offhold {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # wait some time to make sure client is bound
    # sleep 1;

    # start sipp client and send calls 
    sipp -sf ip2ip_uas_recv_hold_offhold.xml ${REMOTEADDR_lo} -i ${LOCALIP_lo} -p ${LOCALPORT}
    # kill every one
    # bashtrap
}

# SCENARIO 4 Test 1
function test_account_send_transfer {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # wait some time to make sure client is bound
    # sleep 1;

    # process only one registration
    sipp -sf account_uas_register.xml ${REMOTEADDR_ast} -i ${LOCALIP_eth0} -p ${LOCALPORT} -l 1 -m 1

    # start sipp client and send calls 
    sipp -sf account_uas_recv_transfered.xml ${REMOTEADDR_ast} -i ${LOCALIP_eth0} -p ${LOCALPORT} -l 1

    # kill every one
    # bashtrap
}


# SCENARIO 5 Test 1
function test_ip2ip_send_refused {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # start sipp server to receive calls from sflphone and then hangup
    sipp -sf ip2ip_uac_send_refused.xml ${REMOTEADDR_lo} -s ${REMOTEADDR_lo} -i ${LOCALIP_lo} -p ${LOCALPORT} -l 1

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # run python client and script to make calls
    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # kill every one
    bashtrap 
}


# SCENARIO 6 Test 1
function test_mult_ip2ip_send_hangup {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # start sipp server to receive calls from sflphone
    sipp -sf ip2ip_uac_send_hangup.xml 127.0.0.1:5060 -i 127.0.0.1 -p 5062 -l 1 -m 10 
    sipp -sf ip2ip_uac_send_hangup.xml 127.0.0.1:5060 -i 127.0.0.1 -p 5064 -l 1 -m 10
    sipp -sf ip2ip_uac_send_hangup.xml 127.0.0.1:5060 -i 127.0.0.1 -p 5066 -l 1 -m 10
    # sipp -sf ip2ip_uac_send_hangup.xml ${REMOTEADDR_lo} -i ${LOCALIP_lo} -p ${LOCALPORT} -l 1 -m 10

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # run python client and script to make calls
    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # kill every one
    # bashtrap
}


# SCENARIO 6 Test 2
function test_mult_ip2ip_recv_peer_hangup {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # start sipp server to receive calls from sflphone
    sipp -sf ip2ip_uac_send_hangup.xml 127.0.0.1 -i 127.0.0.1:5060 -p 5062
    sipp -sf ip2ip_uac_send_hangup.xml 127.0.0.1 -i 127.0.0.1:5060 -p 5064
    sipp -sf ip2ip_uac_send_hangup.xml 127.0.0.1 -i 127.0.0.1:5060 -p 5066
    # sipp -sf ip2ip_uas_recv_peer_hungup.xml ${REMOTEADDR_lo} -i ${LOCALIP_lo} -p ${LOCALPORT}

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # run python client and script to make calls
    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # kill every one
    # bashtrap
}


# function called if CTRL-C detected 
bashtrap()
{
    killall sipp
    killall sflphoned
}


# ============================ Test Suite ============================



# SCENARIO 1: (IP2IP) Normal flow calls

# Test 1: - Send an IP2IP call
#         - Hangup
test_ip2ip_send_hangup

# Test 2: - Send an IP2IP call
#         - Peer Hangup
# test_ip2ip_send_peer_hungup

# Test 3: - Receive an IP2IP call
#         - Hangup
# test_ip2ip_recv_hangup

# Test 4: - Receive an IP2IP call
#         - Peer Hangup
# test_ip2ip_recv_peer_hungup



# SCENARIO 2: (ACCOUNT) Normal flow calls

# Test 1: - Send an ACCOUNT call
#         - Hangup
# test_account_send_hangup

# Test 2: - Send an ACCOUNT call
#         - Peer Hangup
# test_account_send_peer_hungup

# Test 3: - Receive an ACCOUNT call
#         - Hangup
# test_account_recv_hangup

# Test 4: - Receive an ACCOUNT call
#         - Peer Hangup
# test_account_recv_peer_hungup



# SCENARIO 3: Hold/offHold calls (Account)

# Test 1: - Send an IP2IP call
#         - Put this call on HOLD
#         - Off HOLD this call
#         - Hangup
# test_ip2ip_send_hold_offhold



# SCENARIO 4: Transfer calls (Account)

# Test 1: - Send an IP2IP call
#         - Transfer this call to another sipp instance
#         - Hangup
# test_account_send_transfer


#SCENARIO 5: Refuse call (IP2IP)

# Test 1: - Receive a call
#         - Refuse (hangup without answer)
# test_ip2ip_send_refused


#SCENARIO 6: Multiple simultaneous Call

# Test 1: -
# test_mult_ip2ip_send_hangup

# Test 2: -
# test_mult_ip2ip_recv_peer_hangup