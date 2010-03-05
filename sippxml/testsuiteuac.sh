#!/bin/bash
# sleep 5;

SERVERPORT=5062

function test_ip2ipcall {

    #start sipp server to receive calls from sflphone
    sipp -sf ip2ipcalluas.xml -p ${SERVERPORT} &

    #start sflphoned
    /usr/lib/sflphone/sflphoned& 

    #wait some time to make sure sflphone is started
    sleep 2;

    #run python client and associated script
    python ../tools/pysflphone/pysflphone_testdbus.py

    #kill every one
    killall sipp
    killall sflphoned
}

function test_accountcall {

    sipp -sf accountcalluac.xml 192.168.50.79 -i 192.168.50.182 -p ${SERVERPORT}

}

# function called if CTRL-C detected
bashtrap()
{
    killall sipp
    killall sflphoned
}


# Here Start the Test suite

test_ip2ipcall