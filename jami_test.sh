#!/bin/bash
#=================================================================================================================================
#description:    This script will call a dbus instance and jami_test.py script for monitoring on nagios
#authors:        Mohamed Fenjiro <mohamed.fenjiro@savoirfairelinux.com>
#                Sébastien Blin <sebastien.blin@savoirfairelinux.com>
#usage:          ./script.sh [-h] [--messages MESSAGES] [--duration DURATION] [--interval INTERVAL] [--peer PEER] [--calls CALLS]
#notes:          see https://wiki.savoirfairelinux.com/wiki/Jami-monitorpeervm-01.mtl.sfl#Directives for more info
#=================================================================================================================================

if test -z "$DBUS_SESSION_BUS_ADDRESS" ; then
	eval `dbus-launch --sh-syntax`
	echo "D-Bus per-session daemon address is:"
	echo "$DBUS_SESSION_BUS_ADDRESS" > ~/dbus_address
fi

TIMESTAMP=`date "+%Y-%m-%d-%H:%M:%S"`

./bin/dring -cd &

if [ ! -f "~/dbus_address" ]; then
	echo "No daemon detected. Abort";
fi

TESTER="./tools/dringctrl/jami_test.py"
DBUS_ADDR=$(cat ~/dbus_address)
DBUS_SESSION_BUS_ADDRESS=${DBUS_ADDR} ${TESTER} "$@"
