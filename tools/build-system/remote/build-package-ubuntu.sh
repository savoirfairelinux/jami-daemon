#!/bin/bash
#
# @author: Yun Liu <yun.liu@savoirfairelinux.com>, Julien Bonjean <julien.bonjean@savoirfairelinux.com>
#
# Refer to http://www.sflphone.org for futher information
#

PLATFORM="ubuntu"

ROOT_DIR="/home/sflphone/sflphone-packaging"
BUILD_DIR="${ROOT_DIR}/build"
DIST_DIR="${ROOT_DIR}/dists"
REPOSITORY_ARCHIVE="${BUILD_DIR}/sflphone.tar.gz"
REPOSITORY_DIR="${BUILD_DIR}/sflphone"
REPOSITORY_SFLPHONE_COMMON_DIR="${REPOSITORY_DIR}/sflphone-common"
REPOSITORY_SFLPHONE_CLIENT_KDE_DIR="${REPOSITORY_DIR}/sflphone-client-kde"
REPOSITORY_SFLPHONE_CLIENT_GNOME_DIR="${REPOSITORY_DIR}/sflphone-client-gnome"
USER="sflphone"
DIST_APPEND="-daily"
RELEASE_MODE=$1
VERSION_APPEND=
EDITOR=echo
export EDITOR

#########################
# BEGIN
#########################

WHO=`whoami`

if [ "${WHO}" != "${USER}" ]; then
        echo "!! Please use user ${USER} to run this script"
        exit -1;
fi

if [ ${RELEASE_MODE} ]; then
        echo "Release mode : ${RELEASE_MODE}"
	if [ "${RELEASE_MODE}" = "release" ]; then
		DIST_APPEND=""
	else
		DIST_APPEND="-testing"
		VERSION_APPEND="~${RELEASE_MODE}"
	fi
else
        echo "Snapshot mode"
fi

cd ${ROOT_DIR}

if [ "$?" -ne "0" ]; then
        echo " !! Cannot cd to working directory"
        exit -1
fi

# decompress reppository
echo "Untar repository"
cd ${BUILD_DIR} && tar xf ${REPOSITORY_ARCHIVE}

if [ "$?" -ne "0" ]; then
        echo " !! Cannot untar repository"
        exit -1
fi

echo "Switch to internal logging"

# get system parameters
ARCH_FLAG=`getconf -a|grep LONG_BIT | sed -e 's/LONG_BIT\s*//'`
OS_VERSION=`lsb_release -d -s -c | sed -e '1d'`
VER=`cd ${REPOSITORY_DIR} && git describe --tag HEAD  | cut -d "/" -f2 | cut -d "-" -f1`
FULL_VER=`cd ${REPOSITORY_DIR} && git describe --tag HEAD  | cut -d "/" -f2 | cut -d "-" -f1-2`

# define log files
GLOBAL_LOG=${ROOT_DIR}/sflphone-${OS_VERSION}-${ARCH_FLAG}.log
PACKAGING_LOG=${ROOT_DIR}/sflphone-debuild-${OS_VERSION}-${ARCH_FLAG}.log

# open log file
exec 3<>${GLOBAL_LOG}

# redirect outputs (stdout & stderr)
exec 1>&3
exec 2>&3

echo "SFLPhone version is ${VER}"

# generate the changelog, according to the distribution
echo "Generate changelogs"
sed -i 's/SYSTEM/'${OS_VERSION}'/g' ${REPOSITORY_SFLPHONE_COMMON_DIR}/debian/changelog && \
sed -i 's/SYSVER/0ubuntu1/g' ${REPOSITORY_SFLPHONE_COMMON_DIR}/debian/changelog && \
 # sed -i 's/SYSTEM/'${OS_VERSION}'/g' ${REPOSITORY_SFLPHONE_CLIENT_KDE_DIR}/debian/changelog && \
 # sed -i 's/SYSVER/0ubuntu1/g' ${REPOSITORY_SFLPHONE_CLIENT_KDE_DIR}/debian/changelog && \
 sed -i 's/SYSTEM/'${OS_VERSION}'/g' ${REPOSITORY_SFLPHONE_CLIENT_GNOME_DIR}/debian/changelog && \
 sed -i 's/SYSVER/0ubuntu1/g' ${REPOSITORY_SFLPHONE_CLIENT_GNOME_DIR}/debian/changelog

if [ "$?" -ne "0" ]; then
	echo "!! Cannot generate changelogs"
	exit -1
fi

# copy the appropriate control file based on different archtecture
echo "Generate control files"
cp ${REPOSITORY_SFLPHONE_COMMON_DIR}/debian/control.$OS_VERSION ${REPOSITORY_SFLPHONE_COMMON_DIR}/debian/control && \
 # cp ${REPOSITORY_SFLPHONE_CLIENT_KDE_DIR}/debian/control.$OS_VERSION ${REPOSITORY_SFLPHONE_CLIENT_KDE_DIR}/debian/control && \
 cp ${REPOSITORY_SFLPHONE_CLIENT_GNOME_DIR}/debian/control.$OS_VERSION ${REPOSITORY_SFLPHONE_CLIENT_GNOME_DIR}/debian/control

if [ "$?" -ne "0" ]; then
        echo "!! Cannot generate control files"
        exit -1
fi

# provide prerequisite directories used by debuild
echo "Build sflphone packages on Ubuntu $OS_VERSION $ARCH_FLAG bit architecture...."
cp -r ${REPOSITORY_SFLPHONE_COMMON_DIR} ${BUILD_DIR}/sflphone-common && \
cp -r ${REPOSITORY_SFLPHONE_COMMON_DIR} ${BUILD_DIR}/sflphone-common-$VER.orig && \
 # cp -r ${REPOSITORY_SFLPHONE_CLIENT_KDE_DIR} ${BUILD_DIR}/sflphone-client-kde-$VER.orig && \
 cp -r ${REPOSITORY_SFLPHONE_CLIENT_GNOME_DIR} ${BUILD_DIR}/sflphone-client-gnome-$VER.orig && \
# do a cp to because path must remain for client compilation
mv ${REPOSITORY_SFLPHONE_COMMON_DIR} ${BUILD_DIR}/sflphone-common-$VER && \
 # mv ${REPOSITORY_SFLPHONE_CLIENT_KDE_DIR} ${BUILD_DIR}/sflphone-client-kde-$VER && \
 mv ${REPOSITORY_SFLPHONE_CLIENT_GNOME_DIR} ${BUILD_DIR}/sflphone-client-gnome-$VER

# build package sflphone-common
cd ${BUILD_DIR}/sflphone-common-$VER/debian && \
debuild -us -uc >${PACKAGING_LOG} 2>&1

if [ "$?" -ne "0" ]; then
        echo "!! Cannot generate package sflphone-common"
        exit -1
fi

# build package sflphone-client-gnome
cd ${BUILD_DIR}/sflphone-client-gnome-$VER/debian && \
debuild -us -uc >${PACKAGING_LOG} 2>&1

if [ "$?" -ne "0" ]; then
        echo "!! Cannot generate package sflphone-client-gnome"
        exit -1
fi

# build package sflphone-client-kde
# cd ${BUILD_DIR}/sflphone-client-kde-$VER/debian && \
# debuild -us -uc >${PACKAGING_LOG} 2>&1

# if [ "$?" -ne "0" ]; then
#         echo "!! Cannot generate package sflphone-client-kde"
#         exit -1
# fi

# move to dist
echo "Deploy files in dist directories"
BINARY_DIR=""
if [ "${ARCH_FLAG}" -eq "32" ]; then
	BINARY_DIR="binary-i386"
else
	BINARY_DIR="binary-amd64"
fi

mkdir -p ${DIST_DIR}/${OS_VERSION}${DIST_APPEND}/universe/source
mkdir -p ${DIST_DIR}/${OS_VERSION}${DIST_APPEND}/universe/${BINARY_DIR}

mv ${BUILD_DIR}/sflphone*.deb ${DIST_DIR}/${OS_VERSION}${DIST_APPEND}/universe/${BINARY_DIR} && \
mv ${BUILD_DIR}/sflphone*.dsc ${DIST_DIR}/${OS_VERSION}${DIST_APPEND}/universe/source/ && \
mv ${BUILD_DIR}/sflphone*.build ${DIST_DIR}/${OS_VERSION}${DIST_APPEND}/universe/source/ && \
mv ${BUILD_DIR}/sflphone*.changes ${DIST_DIR}/${OS_VERSION}${DIST_APPEND}/universe/source/ && \
mv ${BUILD_DIR}/sflphone*.orig.tar.gz ${DIST_DIR}/${OS_VERSION}${DIST_APPEND}/universe/source/ && \
mv ${BUILD_DIR}/sflphone*.diff.gz ${DIST_DIR}/${OS_VERSION}${DIST_APPEND}/universe/source/

if [ "$?" -ne "0" ]; then
        echo "!! Cannot copy dist files"
        exit -1
fi

echo "All done"

# close file descriptor
exec 3>&-

exit 0

