#!/bin/bash
#####################################################
# File Name: launch-build-machine-jenkins.sh
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info)
#
# Creation Date: 2009-10-20
# Last Modified: 2010-04-22 16:42:57 -0400
#####################################################

set -x

#Check dependencies

# Download the KDE client release script
if  ! command -v curl ; then
   echo Please install curl
   exit 1
fi
# Merge the KDE translation files
if  ! command -v ruby ; then
   echo Please install ruby
   exit 1
fi
# Download the KDE client and scripts
if  ! command -v git ; then
   echo Please install git
   exit 1
fi
# Fetch KDE translations, the gnome client use bzr
if  ! command -v svn ; then
   echo Please install svn
   exit 1
fi


. `dirname $0`/setenv.sh

IS_RELEASE=
VERSION_INDEX="1"
IS_KDE_CLIENT=
DO_PUSH=1
DO_LOGGING=1
DO_UPLOAD=1
SNAPSHOT_TAG=`date +%Y%m%d`
TAG_NAME_PREFIX=
VERSION_NUMBER="1.4.1"

LAUNCHPAD_PACKAGES=("sflphone-daemon" "sflphone-kde" "sflphone-gnome" "sflphone-plugins" "sflphone-daemon-video" "sflphone-gnome-video")

echo
echo "    /***********************\\"
echo "    | SFLPhone build system |"
echo "    \\***********************/"
echo


for PARAMETER in $*
do
        case ${PARAMETER} in
        --help)
                echo
                echo "Options :"
                echo " --skip-push"
                echo " --skip-upload"
                echo " --kde-client"
                echo " --no-logging"
                echo " --release"
                echo " --version-index=[1,2,...]"
                echo
                exit 0;;
        --skip-push)
                unset DO_PUSH;;
        --skip-upload)
                unset DO_UPLOAD;;
        --kde-client)
                IS_KDE_CLIENT=1;;
        --no-logging)
                unset DO_LOGGING;;
        --release)
                IS_RELEASE=1;;
        --version-index=*)
                VERSION_INDEX=(${PARAMETER##*=});;
        *)
                echo "Unknown parameter : ${PARAMETER}"
                exit -1;;
        esac
done

#########################
# LAUNCHPAD
#########################

# change to working directory
cd ${LAUNCHPAD_DIR}

if [ "$?" -ne "0" ]; then
        echo " !! Cannot cd to launchpad directory"
        exit -1
fi

# logging
if [ ${DO_LOGGING} ]; then

	rm -f ${ROOT_DIR}/packaging.log >/dev/null 2>&1

	# open file descriptor
	exec 3<> ${ROOT_DIR}/packaging.log

	# redirect outputs (stdout & stderr)
	exec 1>&3
	exec 2>&3
fi

if [ ${RELEASE_MODE} ]; then
	echo "Release mode"
else
	echo "Snapshot mode"
fi

if [ ${IS_KDE_CLIENT} ]; then
	TAG_NAME_PREFIX="kde."
fi

#########################
# COMMON PART
#########################

cd ${REFERENCE_REPOSITORY}

echo "Update reference sources"
git checkout . && git checkout -f master && git pull

# Get the version
CURRENT_RELEASE_TAG_NAME=`git describe --tags --abbrev=0`
PREVIOUS_RELEASE_TAG_NAME=`git describe --tags --abbrev=0 ${CURRENT_RELEASE_TAG_NAME}^`

CURRENT_RELEASE_COMMIT_HASH=`git show --pretty=format:"%H" -s ${CURRENT_RELEASE_TAG_NAME} | tail -n 1`
PREVIOUS_RELEASE_COMMIT_HASH=`git show --pretty=format:"%H" -s ${PREVIOUS_RELEASE_TAG_NAME} | tail -n 1`
CURRENT_COMMIT=`git show --pretty=format:"%H"  -s | tail -n 1`
CURRENT_RELEASE_TYPE=${CURRENT_RELEASE_TAG_NAME##*.}
PREVIOUS_RELEASE_TYPE=${PREVIOUS_RELEASE_TAG_NAME##*.}
if [ ${IS_KDE_CLIENT} ]; then
	CURRENT_RELEASE_VERSION=${CURRENT_RELEASE_TAG_NAME%.*}
	CURRENT_RELEASE_VERSION=${CURRENT_RELEASE_VERSION#*.}
	PREVIOUS_VERSION=${PREVIOUS_RELEASE_TAG_NAME%.*}
	PREVIOUS_VERSION=${PREVIOUS_VERSION#*.}
else
	CURRENT_RELEASE_VERSION=${CURRENT_RELEASE_TAG_NAME}
	PREVIOUS_VERSION=${PREVIOUS_RELEASE_TAG_NAME}
fi


echo "Retrieve build info"
# retrieve info we may need
if [ ${IS_KDE_CLIENT} ]; then
	TAG_NAME_PREFIX="kde."
	LAUNCHPAD_PACKAGES=( "sflphone-kde" )
fi


cd ${LAUNCHPAD_DIR}

COMMIT_HASH_BEGIN=""
COMMIT_HASH_END=""
SOFTWARE_VERSION=""
LAUNCHPAD_CONF_PREFIX=""

if [ ${IS_RELEASE} ]; then
	SOFTWARE_VERSION="${CURRENT_RELEASE_VERSION}"
	COMMIT_HASH_BEGIN="${PREVIOUS_RELEASE_COMMIT_HASH}"
	LAUNCHPAD_CONF_PREFIX="sflphone"
else
	SOFTWARE_VERSION="${VERSION_NUMBER}-rc${SNAPSHOT_TAG}"
	COMMIT_HASH_BEGIN="${CURRENT_RELEASE_COMMIT_HASH}"
	LAUNCHPAD_CONF_PREFIX="sflphone-nightly"
fi

VERSION="${SOFTWARE_VERSION}~ppa${VERSION_INDEX}~SYSTEM"

echo "Clean build directory"
git clean -f -x ${LAUNCHPAD_DIR}/* >/dev/null
git checkout ${LAUNCHPAD_DIR}

# If release, checkout the latest tag
if [ ${IS_RELEASE} ]; then
	git checkout ${CURRENT_RELEASE_TAG_NAME}

    # When we need to apply an emergency patch for the release builds
    # This should only be used to temporarily patch packaging tools, not
    # daemon/client code (or anything else that build_tarball would grab).
    if [ -d /tmp/sflphone_release_patch ]; then
        echo "Applying patch(es) to packaging tools..."
        git apply --verbose /tmp/sflphone_release_patch/*
        rm -rf /tmp/sflphone_release_patch
        REQUIRE_RESET=1
    fi
fi

get_dir_name() {
    case $1 in
        sflphone-daemon)
        echo daemon
        ;;
        sflphone-daemon-video)
        echo daemon
        ;;
        sflphone-plugins)
        echo plugins
        ;;
        sflphone-gnome)
        echo gnome
        ;;
        sflphone-gnome-video)
        echo gnome
        ;;
        sflphone-kde)
        echo kde
        ;;
        *)
        exit 1
        ;;
    esac
}

# Looping over the packages
for LAUNCHPAD_PACKAGE in ${LAUNCHPAD_PACKAGES[*]}
do
	echo " Package: ${LAUNCHPAD_PACKAGE}"

	echo "  --> Clean old sources"
	git clean -f -x ${LAUNCHPAD_DIR}/${LAUNCHPAD_PACKAGE}/* >/dev/null

	DEBIAN_DIR="${LAUNCHPAD_DIR}/${LAUNCHPAD_PACKAGE}/debian"

	echo "  --> Retrieve new sources"
	DIRNAME=`get_dir_name ${LAUNCHPAD_PACKAGE}`
	cp -r ${REFERENCE_REPOSITORY}/${DIRNAME}/* ${LAUNCHPAD_DIR}/${LAUNCHPAD_PACKAGE}

	echo "  --> Update software version number (${SOFTWARE_VERSION})"
	echo "${SOFTWARE_VERSION}" > ${LAUNCHPAD_DIR}/${LAUNCHPAD_PACKAGE}/VERSION

	echo "  --> Update debian changelog"

cat << END > ${WORKING_DIR}/sfl-git-dch.conf
WORKING_DIR="${REFERENCE_REPOSITORY}"
SOFTWARE="${LAUNCHPAD_PACKAGE}"
VERSION="${VERSION}"
DISTRIBUTION="SYSTEM"
CHANGELOG_FILE="${DEBIAN_DIR}/changelog"
COMMIT_HASH_BEGIN="${COMMIT_HASH_BEGIN}"
COMMIT_HASH_END="${COMMIT_HASH_END}"
IS_RELEASE=${IS_RELEASE}
export DEBFULLNAME="Emmanuel Milou"
export DEBEMAIL="emmanuel.milou@savoirfairelinux.com"
export EDITOR="echo"
END

	${WORKING_DIR}/sfl-git-dch-2.sh ${WORKING_DIR}/sfl-git-dch.conf ${REFERENCE_REPOSITORY}/${DIRNAME}/
	if [ "$?" -ne "0" ]; then
		echo "!! Cannot update debian changelogs"
		exit -1
	fi

	if [ "${LAUNCHPAD_PACKAGE}"  == "sflphone-kde" ]; then
		version_kde=$(echo ${VERSION}  | grep -e '[0-9]*\.[0-9.]*' -o | head -n1)
		sed -i -e "s/Standards-Version: [0-9.A-Za-z]*/Standards-Version: ${version_kde}/" ${LAUNCHPAD_DIR}/${LAUNCHPAD_PACKAGE}/debian/control
		tar -C ${LAUNCHPAD_DIR}/ -cjf ${LAUNCHPAD_DIR}/sflphone-kde_${version_kde}.orig.tar.bz2  ${LAUNCHPAD_PACKAGE}
	fi

	rm -f ${WORKING_DIR}/sfl-git-dch.conf >/dev/null 2>&1

	cd ${LAUNCHPAD_DIR}

	cp ${DEBIAN_DIR}/changelog ${DEBIAN_DIR}/changelog.generic

	for LAUNCHPAD_DISTRIBUTION in ${LAUNCHPAD_DISTRIBUTIONS[*]}
	do

		LOCAL_VERSION="${SOFTWARE_VERSION}~ppa${VERSION_INDEX}~${LAUNCHPAD_DISTRIBUTION}"

		cp ${DEBIAN_DIR}/changelog.generic ${DEBIAN_DIR}/changelog

		sed -i "s/SYSTEM/${LAUNCHPAD_DISTRIBUTION}/g" ${DEBIAN_DIR}/changelog

		cd ${LAUNCHPAD_DIR}/${LAUNCHPAD_PACKAGE}
		if [ "${DIRNAME}"  == "daemon" ]; then
			mkdir -p contrib/native
			pushd contrib/native
			../bootstrap
			# only fetch it, don't build it
			make iax
			popd
		fi
		if [ "${LAUNCHPAD_PACKAGE}"  != "sflphone-kde" ]; then
			./autogen.sh
		fi
		debuild -S -sa -kF5362695
		cd ${LAUNCHPAD_DIR}

		if [ ${DO_UPLOAD} ] ; then
			dput -f --debug --no-upload-log -c ${LAUNCHPAD_DIR}/dput.conf ${LAUNCHPAD_CONF_PREFIX}-${LAUNCHPAD_DISTRIBUTION} ${LAUNCHPAD_PACKAGE}_${LOCAL_VERSION}_source.changes
		fi
	done

	cp ${DEBIAN_DIR}/changelog.generic ${DEBIAN_DIR}/changelog
done

# if push is activated
#if [[ ${DO_PUSH} && ${IS_RELEASE} ]];then
#	echo " Doing commit"
#	git commit -m "[#1262] Released ${SOFTWARE_VERSION}" .
#
#	echo " Pushing commit"
#	git push origin release
#fi


# Archive source tarball for Debian maintainer
. ${WORKING_DIR}/build_tarball.sh ${SOFTWARE_VERSION}

# Undo any modifications caused by temporary patches
if [ "$REQUIRE_RESET" == "1" ]; then
	git reset --hard
fi

# close file descriptor
exec 3>&-

exit 0
