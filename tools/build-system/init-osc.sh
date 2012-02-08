#!/bin/bash
#####################################################
# File Name: init-osc.sh
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info)
#
# Creation Date: 2009-11-02
# Last Modified:
#####################################################

OSC_REPOSITORY="${ROOT_DIR}/sflphone-osc"

LAUNCHPAD_PACKAGES=( "sflphone-client-gnome" "sflphone-common" )

cd ${OSC_REPOSITORY}

for LAUNCHPAD_PACKAGE in ${LAUNCHPAD_PACKAGES[*]}
do
	yes | osc init home:jbonjean:sflphone ${LAUNCHPAD_PACKAGE}
done

exit 0
