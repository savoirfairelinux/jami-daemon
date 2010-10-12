#!/bin/bash
#####################################################
# File Name: setenv.sh
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info) 
#
# Creation Date: 2009-12-15
# Last Modified: 2009-12-15 18:16:52 -0500
#####################################################

# home directory
export ROOT_DIR=${HOME}

# gpg passphrase file
export GPG_FILE="${ROOT_DIR}/.gpg-sflphone"

export EDITOR="echo"

export REFERENCE_REPOSITORY="${ROOT_DIR}/sflphone-source-repository"

export WORKING_DIR="${ROOT_DIR}/sflphone-build-repository/tools/build-system"
export LAUNCHPAD_DIR="${WORKING_DIR}/launchpad"
LAUNCHPAD_DISTRIBUTIONS=( "karmic" "lucid" "maverick" )
export LAUNCHPAD_DISTRIBUTIONS

