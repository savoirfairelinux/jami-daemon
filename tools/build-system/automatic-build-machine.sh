#!/bin/bash
#####################################################
# File Name: send-emails.sh
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info)
#
# Creation Date: 2009-04-20
# Last Modified:
#####################################################

TAG=`date +%Y-%m-%d`
ROOT_DIR="/home/projects/sflphone"
SCRIPTS_DIR="${ROOT_DIR}/build-system"

cd ${SCRIPTS_DIR}

${SCRIPTS_DIR}/launch-build-machine.sh $*

${SCRIPTS_DIR}/send-emails.sh $?

exit 0

