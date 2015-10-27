#!/bin/bash
#####################################################
# File Name: make-telify-package.sh
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info)
#
# Creation Date: 2009-12-15
# Last Modified: 2009-12-15 18:16:47 -0500
#####################################################

#set -x

. `dirname $0`/setenv.sh

# change to working directory
cd ${LAUNCHPAD_DIR}

if [ "$?" -ne "0" ]; then
        echo " !! Cannot cd to launchpad directory"
        exit -1
fi

cd ${REFERENCE_REPOSITORY}

for LAUNCHPAD_DISTRIBUTION in ${LAUNCHPAD_DISTRIBUTIONS[*]}
do
	LOCAL_VERSION="${SOFTWARE_VERSION}~ppa${VERSION_INDEX}~${LAUNCHPAD_DISTRIBUTION}"

	cp ${DEBIAN_DIR}/control ${DEBIAN_DIR}/control
	cp ${DEBIAN_DIR}/changelog.generic ${DEBIAN_DIR}/changelog

	sed -i "s/SYSTEM/${LAUNCHPAD_DISTRIBUTION}/g" ${DEBIAN_DIR}/changelog

	cd ${LAUNCHPAD_DIR}/${LAUNCHPAD_PACKAGE}
	./autogen.sh
	debuild -S -sa -kFDFE4451
	cd ${LAUNCHPAD_DIR}

	if [ ${DO_UPLOAD} ] ; then
		dput -f -c ${LAUNCHPAD_DIR}/dput.conf ${LAUNCHPAD_CONF_PREFIX}-${LAUNCHPAD_DISTRIBUTION} ${LAUNCHPAD_PACKAGE}_${LOCAL_VERSION}_source.changes
	fi
done

