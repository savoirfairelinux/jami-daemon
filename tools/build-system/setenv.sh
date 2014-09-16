#!/bin/bash
# Export environment variables for launch-build-machine-jenkins.sh script.

# home directory
export ROOT_DIR=${HOME}

# gpg passphrase file
export GPG_FILE="${WORKSPACE}/.gpg-sflphone"

export EDITOR="echo"

export REFERENCE_REPOSITORY="${WORKSPACE}"

# In case the script is executed manually, replace the variables set by Jenkins
WORKSPACE=${WORKSPACE:=.}

export WORKING_DIR="${WORKSPACE}/tools/build-system"
export LAUNCHPAD_DIR="${WORKING_DIR}/launchpad"
export LAUNCHPAD_DISTRIBUTIONS=("trusty utopic")
