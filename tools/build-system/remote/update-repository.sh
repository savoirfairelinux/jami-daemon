#!/bin/bash

# Script to update the debian repository description files (Release, Packages, Sources)
#
# @author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
# @date: 2008 Jan 16  

ROOT_DIR="/var/repos/sflphone/debian"
USER="sflphone-package-manager"

export LANG=en_CA.UTF-8
export LC_ALL=en_CA.UTF-8

WHO=`whoami`

if [ "${WHO}" != "${USER}" ]; then
	echo "!! Please use user ${USER} to run this script"
	exit -1;
fi

echo  "Check GPG agent"
pgrep -u "sflphone-package-manager" gpg-agent > /dev/null
if [ "$?" -ne "0" ]; then
	echo "!! GPG agent is not running"
        exit -1
fi
GPG_AGENT_INFO=`cat $HOME/.gpg-agent-info 2> /dev/null`
export ${GPG_AGENT_INFO}

if [ "${GPG_AGENT_INFO}" == "" ]; then
	echo "!! Cannot get GPG agent info"
	exit -1
fi

apt-ftparchive generate conf/apt-ftparchive.conf

echo "Generate the description file for each distribution"
apt-ftparchive -c conf/apt-hardy-release.conf release dists/hardy > dists/hardy/Release
apt-ftparchive -c conf/apt-hardy-testing.conf release dists/hardy-testing > dists/hardy-testing/Release
apt-ftparchive -c conf/apt-hardy-daily.conf release dists/hardy-daily > dists/hardy-daily/Release

apt-ftparchive -c conf/apt-intrepid-release.conf release dists/intrepid > dists/intrepid/Release
apt-ftparchive -c conf/apt-intrepid-testing.conf release dists/intrepid-testing > dists/intrepid-testing/Release
apt-ftparchive -c conf/apt-intrepid-daily.conf release dists/intrepid-daily > dists/intrepid-daily/Release

apt-ftparchive -c conf/apt-jaunty-release.conf release dists/jaunty > dists/jaunty/Release
apt-ftparchive -c conf/apt-jaunty-testing.conf release dists/jaunty-testing > dists/jaunty-testing/Release
apt-ftparchive -c conf/apt-jaunty-daily.conf release dists/jaunty-daily > dists/jaunty-daily/Release

echo "Sign the Release files"

rm -f dists/intrepid/Release.gpg
rm -f dists/intrepid-testing/Release.gpg
rm -f dists/intrepid-daily/Release.gpg
gpg --no-tty -u C842D122 --output dists/intrepid/Release.gpg -ba dists/intrepid/Release
gpg --no-tty -u C842D122 --output dists/intrepid-testing/Release.gpg -ba dists/intrepid-testing/Release
gpg --no-tty -u C842D122 --output dists/intrepid-daily/Release.gpg -ba dists/intrepid-daily/Release

rm -f dists/hardy/Release.gpg
rm -f dists/hardy-testing/Release.gpg
rm -f dists/hardy-daily/Release.gpg
gpg --no-tty -u C842D122 --output dists/hardy/Release.gpg -ba dists/hardy/Release
gpg --no-tty -u C842D122 --output dists/hardy-testing/Release.gpg -ba dists/hardy-testing/Release
gpg --no-tty -u C842D122 --output dists/hardy-daily/Release.gpg -ba dists/hardy-daily/Release

rm -f dists/jaunty/Release.gpg
rm -f dists/jaunty-testing/Release.gpg
rm -f dists/jaunty-daily/Release.gpg
gpg --no-tty -u C842D122 --output dists/jaunty/Release.gpg -ba dists/jaunty/Release
gpg --no-tty -u C842D122 --output dists/jaunty-testing/Release.gpg -ba dists/jaunty-testing/Release
gpg --no-tty -u C842D122 --output dists/jaunty-daily/Release.gpg -ba dists/jaunty-daily/Release

echo "All done"
exit 0
