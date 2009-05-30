#!/bin/bash
#####################################################
# File Name: build-packages-opensuse.sh
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info) 
#
# Creation Date: 2009-05-27
# Last Modified: 2009-05-29 15:18:14 -0400
#####################################################

. ../globals

cd ${OPENSUSE_DIR}

if [ "$?" -ne "0" ]; then
        echo " !! Cannot cd to openSUSE directory"
        exit -1
fi

# create build directories
echo "Create directories"
mkdir -p ${BUILD_DIR}/BUILD
mkdir -p ${BUILD_DIR}/RPMS
mkdir -p ${BUILD_DIR}/SOURCES
mkdir -p ${BUILD_DIR}/SPECS
mkdir -p ${BUILD_DIR}/SRPMS

# create rpm macros
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

# create packages
for PACKAGE in ${PACKAGES[@]}
do
	echo "Prepare ${PACKAGE}"

	cd ${REPOSITORY_DIR}

	echo " -> create source archive"
	mv ${PACKAGE} ${PACKAGE}-${VERSION} 2>/dev/null && \
	tar cf ${PACKAGE}.tar.gz ${PACKAGE}-${VERSION} && \
	mv ${PACKAGE}-${VERSION} ${PACKAGE}
	
	if [ "$?" -ne "0" ]; then
		echo "!! Cannot create source archive"
		exit -1
	fi
	
	echo " -> move archive to source directory"
	mv ${PACKAGE}.tar.gz ${BUILD_DIR}/SOURCES

	if [ "$?" -ne "0" ]; then
                echo "!! Cannot move archive"
                exit -1
        fi

	cd ${PACKAGING_DIR}

	echo " -> update spec file"
	sed "s/VERSION/${VERSION}/g" ${PACKAGE}.spec > ${BUILD_DIR}/SPECS/${PACKAGE}.spec

	if [ "$?" -ne "0" ]; then
                echo "!! Cannot update spec file"
                exit -1
        fi
done

# launch build
echo "Launch build"
rpmbuild -ba ${BUILD_DIR}/SPECS/*.spec

if [ "$?" -ne "0" ]; then
	echo "!! Cannot build packages"
	exit -1
fi

