#!/bin/bash
# sleep 5;

SERVERPORT=5062

function test_ip2ip_send_hangup {

    # start sipp server to receive calls from sflphone
    sipp -sf ip2ip_uas_recv_peer_hungup.xml -p ${SERVERPORT}

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # run python client and script to make calls
    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # kill every one
    # bashtrap
}

function test_ip2ip_send_peer_hungup {

    # start sipp server to receive calls from sflphone and then hangup
    sipp -sf ip2ip_uas_recv_hangup.xml -p ${SERVERPORT}

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # run python client and script to make calls
    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # kill every one
    bashtrap 
}


function test_ip2ip_recv_hangup {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # wait some time to make sure client is bound
    # sleep 1;

    # start sipp client and send calls 
    sipp -sf ip2ip_uac_send_peer_hungup.xml 127.0.0.1:5060 -i 127.0.0.1 -p ${SERVERPORT} -l 1

    # kill every one
    # bashtrap 
}

function test_ip2ip_recv_peer_hungup {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # wait some time to make sure client is bound
    # sleep 1;

    # start sipp client and send calls 
    sipp -sf ip2ip_uac_send_hangup.xml 127.0.0.1:5060 -i 127.0.0.1 -p ${SERVERPORT} -l 1

    # kill every one
    # bashtrap 
}

function test_account_send_hangup {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # wait some time to make sure client is bound
    # sleep 1;

    # start sipp client and send calls 
    sipp -sf account_uas_recv_peer_hungup.xml 192.168.50.79 -i 192.168.50.182 -p ${SERVERPORT} -l 1

    # kill every one
    # bashtrap
}

function test_account_recv_peer_hungup {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # wait some time to make sure sflphoned is started
    # sleep 1;

    # python ../tools/pysflphone/pysflphone_testdbus.py &

    # wait some time to make sure client is bound
    # sleep 1;

    # start sipp client and send calls 
    sipp -sf account_uac_send_hangup.xml 192.168.50.79 -i 192.168.50.182 -p ${SERVERPORT} -l 1

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

# SCENARIO 1: Normal flow calls (IP2IP)
# test_ip2ip_send_hangup
# test_ip2ip_send_peer_hungup
# test_ip2ip_recv_hangup
# test_ip2ip_recv_peer_hungup

# SCENARIO X: Normal flow calls (Account)
test_account_send_hangup
# test_account_recv_peer_hungup