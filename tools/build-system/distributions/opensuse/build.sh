#!/bin/bash
#set -x

ROOT_DIR=/tmp/sflphone
SRC_DIR=${HOME}/sflphone
WORKING_DIR=${SRC_DIR}/platform/work
VERSION=0.9.8

#PACKAGES=('sflphone-common' 'sflphone-client-gnome' 'sflphone-client-kde')
#PACKAGES=('sflphone-common')
PACKAGES=('sflphone-client-gnome')

echo "Create directories"
mkdir -p ${ROOT_DIR}/BUILD
mkdir -p ${ROOT_DIR}/RPMS
mkdir -p ${ROOT_DIR}/SOURCES
mkdir -p ${ROOT_DIR}/SPECS
mkdir -p ${ROOT_DIR}/SRPMS

echo "Create RPM macros"
cat > ~/.rpmmacros << STOP
%packager               Julien Bonjean (julien.bonjean@savoirfairelinux.com)
%distribution           Savoir-faire Linux
%vendor                 Savoir-faire Linux

%_signature             gpg
%_gpg_name              Julien Bonjean

%_topdir                ${ROOT_DIR}
%_builddir		%{_topdir}/BUILD
%_rpmdir		%{_topdir}/RPMS
%_sourcedir		%{_topdir}/SOURCES
%_specdir		%{_topdir}/SPECS
%_srcrpmdir		%{_topdir}/SRPMS
STOP


for PACKAGE in ${PACKAGES[@]}
do
	echo "Process ${PACKAGE}"

	cd ${SRC_DIR}

	echo " -> create source archive"
	mv ${PACKAGE} ${PACKAGE}-${VERSION} 2>/dev/null
	tar cf ${PACKAGE}.tar.gz ${PACKAGE}-${VERSION}
	mv ${PACKAGE}-${VERSION} ${PACKAGE}
	
	echo " -> move archive to working directory"
	mv ${PACKAGE}.tar.gz ${ROOT_DIR}/SOURCES

	cd ${WORKING_DIR}

	echo " -> update version in spec file"
	sed -i "s/VERSION/${VERSION}/g" ${PACKAGE}.spec

	echo " -> launch build"
	rpmbuild -ba ${PACKAGE}.spec
done

exit 0
