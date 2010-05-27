#!/bin/bash
#####################################################
# File Name: build-osc.sh
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info) 
#
# Creation Date: 2009-11-02
# Last Modified: 2010-05-27 17:39:58 -0400
#####################################################

ROOT_DIR=${HOME}

OSC_REPOSITORY="${ROOT_DIR}/sflphone-osc/home:jbonjean:sflphone"

LAUNCHPAD_PACKAGES=( "sflphone-client-gnome" "sflphone-common" )
#LAUNCHPAD_PACKAGES=( "sflphone-client-gnome" )
#LAUNCHPAD_PACKAGES=( "sflphone-common" )

REFERENCE_REPOSITORY="${ROOT_DIR}/sflphone-source-repository"
OSC_DIR="${REFERENCE_REPOSITORY}/tools/build-system/osc"

SOFTWARE_VERSION="0.9.8.3"

VERSION_INDEX=1

cd ${OSC_REPOSITORY}

for LAUNCHPAD_PACKAGE in ${LAUNCHPAD_PACKAGES[*]}
do
	cd ${OSC_REPOSITORY}/${LAUNCHPAD_PACKAGE}

	rm -rf ${LAUNCHPAD_PACKAGE}-${SOFTWARE_VERSION}*

	cp -r ${REFERENCE_REPOSITORY}/${LAUNCHPAD_PACKAGE} ${LAUNCHPAD_PACKAGE}-${SOFTWARE_VERSION}

	cp ${OSC_DIR}/${LAUNCHPAD_PACKAGE}* .

	sed -i -e "s/VERSION_INDEX/${VERSION_INDEX}/g" -e "s/VERSION/${SOFTWARE_VERSION}/g" ${LAUNCHPAD_PACKAGE}.spec

	tar czf ${LAUNCHPAD_PACKAGE}-${SOFTWARE_VERSION}.tar.gz ${LAUNCHPAD_PACKAGE}-${SOFTWARE_VERSION}

	rm -rf ${LAUNCHPAD_PACKAGE}-${SOFTWARE_VERSION} 
	
	osc add ${LAUNCHPAD_PACKAGE}-${SOFTWARE_VERSION}.tar.gz
	osc add *.patch

	yes | osc commit --force -m "Version ${SOFTWARE_VERSION}"
done

exit 0
