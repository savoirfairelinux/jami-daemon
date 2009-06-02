#!/bin/bash
#####################################################
# File Name: build-packages.sh
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info) 
#
# Creation Date: 2009-05-29
# Last Modified: 2009-06-01 17:27:25 -0400
#####################################################

. ./globals

if [ "$?" -ne 0 ]; then
	echo "!! Cannot source global file"
	exit -1
fi

cd ${PACKAGING_DIR}

if [ ! ${PACKAGING_DIR} ];then
	echo "!! Cannot go to working directory"
	exit -1
fi

# check if version is ok
if [ ! ${VERSION} ]; then
        echo "!! Cannot detect current version"
        exit -1
fi

# open log file
exec 3<>${LOG_FILE}

# redirect outputs (stdout & stderr)
exec 1>&3
exec 2>&3

echo "SFLPhone version is ${VERSION}"

# check user
if [ "${WHOAMI}" != "${USER}" ]; then
        echo "!! Please use user ${USER} to run this script"
        exit -1;
fi

if [ ${RELEASE_MODE} ]; then
        echo "Release mode : ${RELEASE_MODE}"
else
        echo "Snapshot mode"
fi

# decompress repository
echo "Untar repository"
cd ${BUILD_DIR} && tar xf ${REPOSITORY_ARCHIVE} >/dev/null 2>&1

if [ "$?" -ne "0" ]; then
        echo " !! Cannot untar repository"
        exit -1
fi

# launch distribution specific script
if [ "${DISTRIBUTION}" = "ubuntu" ];then
	echo "Launch packaging for Ubuntu (hardy/intrepid/jaunty)"
	cd ${UBUNTU_DIR} && ./build-packages-ubuntu.sh $*

elif [ "${DISTRIBUTION}" = "opensuse" ]; then
	echo "Launch packaging for openSUSE 11"
	cd ${OPENSUSE_DIR} && ./build-packages-opensuse.sh $*

elif [ "${DISTRIBUTION}" = "mandriva" ]; then
	echo "Launch packaging for Mandriva 2009.1"
	cd ${MANDRIVA_DIR} && ./build-packages-mandriva.sh $*

elif [ "${DISTRIBUTION}" = "fedora" ]; then
	echo "Launch packaging for Fedora 11"
	cd ${FEDORA_DIR} && ./build-packages-fedora.sh $*

else
	echo "!! Cannot detect distribution"
	exit -1
fi

if [ "$?" -ne 0 ]; then
	echo "!! Error in subprocess"
	exit -1
fi

echo "All done"

# close file descriptor
exec 3>&-

exit 0
