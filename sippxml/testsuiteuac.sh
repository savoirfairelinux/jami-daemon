#!/bin/bash
# sleep 5;

SERVERPORT=5062

#start sipp server to receive calls from sflphone
sipp -sf simpleuas.xml -p ${SERVERPORT} &

#start sflphoned
/usr/lib/sflphone/sflphoned& 

#wait some time to make sure sflphone is started
sleep 2;

#run python client and associated script
python ../tools/pysflphone/pysflphone_testdbus.py

#kill every one
killall sipp
killall sflphoned

# function called if CTRL-C detected
bashtrap()
{
    killall sipp
    killall sflphoned
}