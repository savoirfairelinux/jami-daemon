#!/bin/bash
#####################################################
# File Name: setenv.sh
#
# Purpose : Export environment variables for launch-build-machine-jenkins.sh script.
#           Fetch the latest KDE client code from KDE repository
#
# Author: Julien Bonjean (julien@bonjean.info)
#
# Creation Date: 2009-12-15
# Last Modified: 2014-03-21 13:16:52 -0500
#####################################################

# home directory
export ROOT_DIR=${HOME}

# gpg passphrase file
export GPG_FILE="${WORKSPACE}/.gpg-sflphone"

export EDITOR="echo"

export REFERENCE_REPOSITORY="${WORKSPACE}"

# In case the script is executed manually, replace the variables set by Jenkins
if [ "${WORKSPACE}" == "" ]; then
   WORKSPACE="."
fi

export WORKING_DIR="${WORKSPACE}/tools/build-system"
export LAUNCHPAD_DIR="${WORKING_DIR}/launchpad"
LAUNCHPAD_DISTRIBUTIONS=("quantal" "saucy" "trusty")
export LAUNCHPAD_DISTRIBUTIONS

# Update KDE client
cd ${WORKSPACE}
rm -rf config.ini
rm -rf kde
curl https://projects.kde.org/projects/playground/network/sflphone-kde/repository/revisions/master/raw/data/config.ini > config.ini
git clone http://anongit.kde.org/kde-dev-scripts
ruby kde-dev-scripts/createtarball/create_tarball.rb -n -a sflphone-kde
rm -rf kde-dev-scripts
tar -xpvf sflphone-kde-*.tar.*
rm -rf sflphone-kde-*.tar.*
mv sflphone-kde-* kde
