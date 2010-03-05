#!/bin/bash
# sleep 5;

SERVERPORT=5062

function test_ip2ipcall {

    # start sipp server to receive calls from sflphone
    sipp -sf ip2ipcalluas.xml -p ${SERVERPORT} &

    # start sflphoned
    /usr/lib/sflphone/sflphoned& 

    # wait some time to make sure sflphoned is started
    sleep 2;

    # run python client and script to make calls
    python ../tools/pysflphone/pysflphone_testdbus.py

    # kill every one
    killall sipp
    killall sflphoned
}

function test_accountcall {

    # start sflphoned
    # /usr/lib/sflphone/sflphoned& 

    # wait some time to make sure sflphoned is started
    #sleep 2;

    # python ../tools/pysflphone/pysflphone_testdbus.py &

    #sleep 2;

    # start sipp client and send calls 
    sipp -sf accountcalluac.xml 192.168.50.79 -i 192.168.50.182 -p ${SERVERPORT} -l 1

    # kill every one
    killall sipp
    killall sflphoned
}

# function called if CTRL-C detected
bashtrap()
{
    killall sipp
    killall sflphoned
}


# Here Start the Test suite

# test_ip2ipcall

test_accountcall