#!/bin/bash


#
# Install the PPA packages
#

# Install the gnome client from the PPA
sudo ./run_package_test.sh -r "http://ppa.launchpad.net/savoirfairelinux/sflphone-nightly/ubuntu" -p sflphone-gnome

# Install the KDE client from the PPA
sudo ./run_package_test.sh -r "http://ppa.launchpad.net/savoirfairelinux/sflphone-nightly/ubuntu" -p sflphone-kde

# Install both clients from the PPA
sudo ./run_package_test.sh -r "http://ppa.launchpad.net/savoirfairelinux/sflphone-nightly/ubuntu" -p sflphone-gnome-video sflphone-kde




#
# Upgrade stock Ubuntu sflphone packages to our PPA
#

# Install the stock gnome client, then upgrade to the PPA
sudo ./run_package_test.sh -r "http://ppa.launchpad.net/savoirfairelinux/sflphone-nightly/ubuntu" \
   -b "apt-get install sflphone-gnome" -p sflphone-gnome

# Install the stock KDE client, then upgrade to the PPA
sudo ./run_package_test.sh -r "http://ppa.launchpad.net/savoirfairelinux/sflphone-nightly/ubuntu" \
   -b "apt-get install sflphone-kde" -p sflphone-kde




#
# Toggle the PPA gnome client video support
#

# Upgrade from non-video Gnome client to Video Gnome client
sudo ./run_package_test.sh -r "http://ppa.launchpad.net/savoirfairelinux/sflphone-nightly/ubuntu" \
   -b "apt-get install sflphone-gnome" -p sflphone-gnome-video

# Downgrade from non-video Gnome client to Video Gnome client
sudo ./run_package_test.sh -r "http://ppa.launchpad.net/savoirfairelinux/sflphone-nightly/ubuntu" \
   -b "apt-get install sflphone-gnome-video" -p sflphone-gnome




#
# List build dependencies
#


# List Gnome client build-dep versus PPA
sudo ./run_package_test.sh -r "http://ppa.launchpad.net/savoirfairelinux/sflphone-nightly/ubuntu"  \
   -b "apt-get build-dep sflphone-gnome -s" -c "apt-get build-dep sflphone-gnome -s"          \
   -c "debtree -b --arch=all --no-recommends --no-conflicts sflphone-gnome > /tmp/graph"      \
   -p debtree -u "dotty tmp/graph"

# List Gnome client (+video) build-dep versus PPA
sudo ./run_package_test.sh -r "http://ppa.launchpad.net/savoirfairelinux/sflphone-nightly/ubuntu"  \
   -b "apt-get build-dep sflphone-gnome -s" -c "apt-get build-dep sflphone-gnome-video -s"    \
   -c "debtree -b --arch=all --no-recommends --no-conflicts sflphone-gnome-video > /tmp/graph"\
   -p debtree -u "dotty tmp/graph"