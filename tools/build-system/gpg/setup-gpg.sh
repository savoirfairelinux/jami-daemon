#!/bin/bash
#####################################################
# File Name: setup-gpg.sh
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info) 
#
# Creation Date: 2009-04-20
# Last Modified:
#####################################################

# pkill gpg-agent

export LANG=en_CA.UTF-8
export LC_ALL=en_CA.UTF-8

echo "Check if GPG key is present"
gpg --list-secret-keys | grep "Savoir-Faire Linux Inc." >/dev/null

if [ "$?" -ne "0" ]; then
       echo "!! GPG private key is not present"
       exit -1
fi

echo  "Check GPG agent"
pgrep -u "sflphone-package-manager" gpg-agent > /dev/null
if [ "$?" -ne "0" ]; then
	echo "Not running, launching it"
        EVAL=`/usr/bin/gpg-agent --daemon --write-env-file $HOME/.gpg-agent-info --default-cache-ttl 2000000000 --max-cache-ttl 2000000000 --pinentry-program /usr/bin/pinentry`
	eval ${EVAL}
fi

if [ "$?" -ne "0" ]; then
       echo "!! Error with GPG agent"
       exit -1
fi

GPG_AGENT_INFO=`cat $HOME/.gpg-agent-info 2> /dev/null`
export ${GPG_AGENT_INFO}

if [ "${GPG_AGENT_INFO}" == "" ]; then
	echo "!! Cannot get GPG agent info"
	exit -1
fi

GPG_TTY=`tty`
export GPG_TTY

touch ./test-gpg
gpg -v --clearsign --use-agent ./test-gpg
rm -f ./test-gpg
rm -f ./test-gpg.asc

exit 0

