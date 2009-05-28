#!/bin/bash
#####################################################
# File Name: build-package-opensuse.sh
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info) 
#
# Creation Date: 2009-05-27
# Last Modified: 2009-05-28 15:35:07 -0400
#####################################################

BUILD_DIR=/tmp/sflphone
SRC_DIR=${HOME}/sflphone-packaging/build/sflphone
WORKING_DIR=${HOME}/sflphone-packaging
VERSION=`cat ${SRC_DIR}/sflphone-common/VERSION`

if [ ! ${VERSION} ]; then
	echo "!! Cannot detect current version"
	exit -1
fi

#PACKAGES=('sflphone-common' 'sflphone-client-gnome' 'sflphone-client-kde')
PACKAGES=('sflphone-common sflphone-client-gnome')

echo "Create directories"
mkdir -p ${BUILD_DIR}/BUILD
mkdir -p ${BUILD_DIR}/RPMS
mkdir -p ${BUILD_DIR}/SOURCES
mkdir -p ${BUILD_DIR}/SPECS
mkdir -p ${BUILD_DIR}/SRPMS

echo "Create RPM macros"
cat > ~/.rpmmacros << STOP
%packager               Julien Bonjean (julien.bonjean@savoirfairelinux.com)
%distribution           Savoir-faire Linux
%vendor                 Savoir-faire Linux

%_signature             gpg
%_gpg_name              Julien Bonjean

%_topdir                ${BUILD_DIR}
%_builddir		%{_topdir}/BUILD
%_rpmdir		%{_topdir}/RPMS
%_sourcedir		%{_topdir}/SOURCES
%_specdir		%{_topdir}/SPECS
%_srcrpmdir		%{_topdir}/SRPMS
STOP


for PACKAGE in ${PACKAGES[@]}
do
	echo "Prepare ${PACKAGE}"

	cd ${SRC_DIR}

	echo " -> create source archive"
	mv ${PACKAGE} ${PACKAGE}-${VERSION} 2>/dev/null
	tar cf ${PACKAGE}.tar.gz ${PACKAGE}-${VERSION}
	mv ${PACKAGE}-${VERSION} ${PACKAGE}
	
	echo " -> move archive to source directory"
	mv ${PACKAGE}.tar.gz ${BUILD_DIR}/SOURCES

	cd ${WORKING_DIR}

	echo " -> update spec file"
	sed "s/VERSION/${VERSION}/g" ${PACKAGE}.spec > ${BUILD_DIR}/SPECS/${PACKAGE}.spec
done

echo "Launch build"
rpmbuild -ba ${BUILD_DIR}/SPECS/*.spec

exit 0

