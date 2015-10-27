#!/bin/bash

NIGHTLY_PPA="http://ppa.launchpad.net/savoirfairelinux/sflphone-nightly/ubuntu"
FAILED=0

# Print an error if a test failed
function() checkResult() {
   RET=$?
   if [ "$RET" != "0" ]; then
      echo !! " [FAILED]"
      let FAILED=$FAILED+1
   fi
}

#
# Install the PPA packages
#

# Install the gnome client from the PPA
sudo ./run_package_test.sh -r $NIGHTLY_PPA -p sflphone-gnome
checkResult

# Install the KDE client from the PPA
sudo ./run_package_test.sh -r $NIGHTLY_PPA -p sflphone-kde
checkResult

# Install both clients from the PPA
sudo ./run_package_test.sh -r $NIGHTLY_PPA -p sflphone-gnome-video sflphone-kde
checkResult




#
# Upgrade stock Ubuntu sflphone packages to our PPA
#

# Install the stock gnome client, then upgrade to the PPA
sudo ./run_package_test.sh -r $NIGHTLY_PPA \
   -b "apt-get install sflphone-gnome" -p sflphone-gnome
checkResult

# Install the stock KDE client, then upgrade to the PPA
sudo ./run_package_test.sh -r $NIGHTLY_PPA \
   -b "apt-get install sflphone-kde" -p sflphone-kde
checkResult




#
# Toggle the PPA gnome client video support
#

# Upgrade from non-video Gnome client to Video Gnome client
sudo ./run_package_test.sh -r $NIGHTLY_PPA \
   -b "apt-get install sflphone-gnome" -p sflphone-gnome-video
checkResult

# Downgrade from non-video Gnome client to Video Gnome client
sudo ./run_package_test.sh -r $NIGHTLY_PPA \
   -b "apt-get install sflphone-gnome-video" -p sflphone-gnome
checkResult




#
# List build dependencies
#


# List Gnome client build-dep versus PPA
sudo ./run_package_test.sh -r $NIGHTLY_PPA  \
   -b "apt-get build-dep sflphone-gnome -s" -c "apt-get build-dep sflphone-gnome -s"          \
   -c "debtree -b --arch=all --no-recommends --no-conflicts sflphone-gnome > /tmp/graph"      \
   -p debtree -u "dotty tmp/graph"
checkResult

# List Gnome client (+video) build-dep versus PPA
sudo ./run_package_test.sh -r $NIGHTLY_PPA  \
   -b "apt-get build-dep sflphone-gnome -s" -c "apt-get build-dep sflphone-gnome-video -s"    \
   -c "debtree -b --arch=all --no-recommends --no-conflicts sflphone-gnome-video > /tmp/graph"\
   -p debtree -u "dotty tmp/graph"
checkResult

# Exit with error 1 if one or more test failed
if [ $FAILED ~= "0" ]; then
   exit 1
fi
