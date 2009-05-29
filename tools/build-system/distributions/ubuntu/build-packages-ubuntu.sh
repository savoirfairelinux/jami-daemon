#!/bin/bash
#
# @author: Yun Liu <yun.liu@savoirfairelinux.com>, Julien Bonjean <julien.bonjean@savoirfairelinux.com>
#
# Refer to http://www.sflphone.org for futher information
#

. ../globals

cd ${UBUNTU_DIR}

if [ "$?" -ne "0" ]; then
        echo " !! Cannot cd to Ubuntu directory"
        exit -1
fi

PACKAGE_SYSVER="0ubuntu1"
FULL_VERSION="${VERSION}-${PACKAGE_SYSVER}"

#########################
# BEGIN
#########################

DIST="dist"
if [ ${RELEASE_MODE} ]; then
	if [ "${RELEASE_MODE}" != "release" ]; then
		DIST="${DIST}-testing"
	fi
else
	DIST="${DIST}-daily"
fi

echo "Do updates"
sudo apt-get update >/dev/null
sudo apt-get upgrade -y >/dev/null


for PACKAGE in ${PACKAGES[@]}
do
        echo "Process ${PACKAGE}"

	echo " -> prepare debian directories"
	mv ${UBUNTU_DIR}/debian-${PACKAGE} ${REPOSITORY_DIR}/${PACKAGE}/debian

	# generate the changelog
	echo " -> generate changelog"
	sed -i 's/SYSTEM/'${OS_VERSION}'/g' ${REPOSITORY_DIR}/${PACKAGE}/debian/changelog && \
	sed -i 's/SYSVER/'${PACKAGE_SYSVER}'/g' ${REPOSITORY_DIR}/${PACKAGE}/debian/changelog

	if [ "$?" -ne "0" ]; then
		echo "!! Cannot generate changelog"
		exit -1
	fi

	# copy the appropriate control file based on architecture
	echo " -> generate control file"
	cp ${REPOSITORY_DIR}/${PACKAGE}/debian/control.$OS_VERSION ${REPOSITORY_DIR}/${PACKAGE}/debian/control && \
 	sed -i "s/VERSION/${FULL_VERSION}/g" ${REPOSITORY_DIR}/${PACKAGE}/debian/control

	if [ "$?" -ne "0" ]; then
	        echo "!! Cannot generate control file"
	        exit -1
	fi

	# provide prerequisite directories used by debuild
	echo " -> prepare directories"
	cp -r ${REPOSITORY_DIR}/${PACKAGE} ${REPOSITORY_DIR}/${PACKAGE}-${FULL_VERSION}.orig && \
	mv ${REPOSITORY_DIR}/${PACKAGE} ${REPOSITORY_DIR}/${PACKAGE}-${FULL_VERSION}

	# build package sflphone-common
	cd ${REPOSITORY_DIR}/${PACKAGE}-${FULL_VERSION}/debian && \
	debuild -us -uc

	if [ "$?" -ne "0" ]; then
	        echo "!! Cannot generate package ${PACKAGE}"
	        exit -1
	fi
done

# move to dist
echo "Deploy files in dist directories"
BINARY_DIR=""
if [ "${ARCH_FLAG}" -eq "32" ]; then
	BINARY_DIR="binary-i386"
else
	BINARY_DIR="binary-amd64"
fi

mkdir -p ${DIST_DIR}/${DIST}/universe/source
mkdir -p ${DIST_DIR}/${DIST}/universe/${BINARY_DIR}

mv ${REPOSITORY_DIR}/sflphone*.deb ${DIST_DIR}/${DIST}/universe/${BINARY_DIR} && \
mv ${REPOSITORY_DIR}/sflphone*.dsc ${DIST_DIR}/${DIST}/universe/source/ && \
mv ${REPOSITORY_DIR}/sflphone*.build ${DIST_DIR}/${DIST}/universe/source/ && \
mv ${REPOSITORY_DIR}/sflphone*.changes ${DIST_DIR}/${DIST}/universe/source/ && \
mv ${REPOSITORY_DIR}/sflphone*.orig.tar.gz ${DIST_DIR}/${DIST}/universe/source/ && \
mv ${REPOSITORY_DIR}/sflphone*.diff.gz ${DIST_DIR}/${DIST}/universe/source/

if [ "$?" -ne "0" ]; then
        echo "!! Cannot copy dist files"
        exit -1
fi

