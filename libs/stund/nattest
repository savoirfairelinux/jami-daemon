#!/bin/bash
# nattest shell script

# set defaults
# Interfaces typically begin eth on Linux, en on Macintosh, le or hme on Solaris
# type ifconfig -a or ifconfig -l to see a list of all interfaces
serverint=eth0
clientint=eth1
serverip1=1.1.1.2
serverip2=1.1.1.3
servermask=255.255.255.0
clientip1=192.168.0.2
clientip2=192.168.0.3
clientmask=255.255.255.0

# print warning, get confirmation
cat nattestwarning.txt
read -p "Are you sure you want to run this? [yes]" confirm
case $confirm in
    [nN]     ) exit;;
    [nN][oO] ) exit;;
esac

# off we go then....
# add second IP address to each interface
ifconfig $serverint $serverip2 netmask 255.255.255.255 alias
ifconfig $clientint $clientip2 netmask 255.255.255.255 alias
# for Solaris, use these instead
# ifconfig ${serverint}:1 $serverip2 netmask $servermask
# ifconfig ${clientint}:1 $clientip2 netmask $clientmask

./stund -h $serverip1 -a $serverip2 -b
# verify server is running

./stunner $serverip1 -i $clientip1 -i2 $clientip2
# process results of stunner and print pass/fail
case "$?" in
	10 ) echo "[PASS] (Address) Restricted Cone NAT with Hairpinning";;
	11 ) echo "[PASS] Port Restricted Cone NAT with Hairpinning";;
	8  ) echo "[No NAT] You have open internet access";;
	2  ) echo "[FAIL] Your (Address) Restricted Cone NAT doesn't do hairpinning";;
	3  ) echo "[FAIL] Your Port Restricted Cone NAT doesn't do hairpinning";;
        -1 ) echo "ERROR! the STUN test program had an error";;
	*  ) echo "[FAIL] You have a NAT or Firewall type which is NOT RECOMMENDED.";;
esac

# cleanup
killall -HUP stund
ifconfig $serverint $serverip2 -alias
ifconfig $clientint $clientip2 -alias
# for Solaris, use these instead
# ifconfig ${serverint}:1 unplumb
# ifconfig ${clientint}:1 unplumb

