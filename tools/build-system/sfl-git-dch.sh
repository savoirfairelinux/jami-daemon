#!/bin/bash
#####################################################
# File Name: sfl-git-dch.sh
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info) 
#
# Creation Date: 2009-05-13
# Last Modified: 2009-07-20 23:03:24 -0400
#####################################################

# set -x

VERSION=$1
RELEASE_MODE=$2

ROOT_DIR="/home/projects/sflphone"
TODEPLOY_DIR="${ROOT_DIR}/sflphone-packaging"
TODEPLOY_BUILD_DIR="${TODEPLOY_DIR}/build"
REPOSITORY_DIR="${TODEPLOY_BUILD_DIR}/sflphone"
SCRIPTS_DIR="${ROOT_DIR}/build-system"
DEBIAN_DIR="${SCRIPTS_DIR}/distributions/ubuntu"
CHANGELOG_FILES=( "${DEBIAN_DIR}/debian-sflphone-common/changelog" "${DEBIAN_DIR}/debian-sflphone-client-gnome/changelog" "${DEBIAN_DIR}/debian-sflphone-client-kde/changelog")

export DEBFULLNAME="SFLphone Automatic Build System"
export DEBEMAIL="team@sflphone.org"
export EDITOR="echo"

cd ${REPOSITORY_DIR}

if [ "$?" -ne "0" ]; then
        echo " !! Cannot cd to working directory"
        exit -1
fi

# get last release tag
LAST_RELEASE_TAG_NAME=`git tag -l "[0-9]\.[0-9]\.[0-9]\.*" | tail -n 1`

if [ "$?" -ne "0" ]; then
	echo " !! Error when retrieving last tag"
	exit -1
fi

# get last release tag
PREVIOUS_RELEASE_TAG_NAME=`git tag -l "[0-9]\.[0-9]\.[0-9]\.*" | tail -n 2 | sed -n '1p;1q'`

if [ "$?" -ne "0" ]; then
	echo " !! Error when retrieving previous revision tag"
	exit -1
fi

echo "Last release tag is : ${LAST_RELEASE_TAG_NAME}"

# get last release commit hash
REF_COMMIT_HASH=
if [ ${RELEASE_MODE} ]; then
	echo "Reference tag is : ${PREVIOUS_RELEASE_TAG_NAME}"
	REF_COMMIT_HASH=`git show --pretty=format:"%H" -s ${PREVIOUS_RELEASE_TAG_NAME} | tail -n 1`
else
	echo "Reference tag is : ${LAST_RELEASE_TAG_NAME}"
	REF_COMMIT_HASH=`git show --pretty=format:"%H" -s ${LAST_RELEASE_TAG_NAME} | tail -n 1`
fi

if [ "$?" -ne "0" ]; then
	echo " !! Error when retrieving last release commit hash"
	exit -1
fi

echo "Reference commit is : ${REF_COMMIT_HASH}"
echo

# use git log to retrieve changelog content
CHANGELOG_CONTENT=`git log --no-merges --pretty=format:"%s" ${REF_COMMIT_HASH}.. | grep -v "\[\#1262\]"`

if [ "$?" -eq "1" ]; then
        echo " !! No new commit since last release"
	CHANGELOG_CONTENT="No new commit"
fi

if [ "$?" -ne "0" ]; then
        echo " !! Error when retrieving changelog content"
        exit -1
fi

# iterate throw changelog files
for CHANGELOG_FILE in ${CHANGELOG_FILES[@]}
do
	echo "Changelog : ${CHANGELOG_FILE}"
	echo
	rm -f ${CHANGELOG_FILE}.dch >/dev/null 2>&1	

	# if previous entry is a snapshot, remove it
	sed -n 's/ //g;3p;3q' ${CHANGELOG_FILE} | grep "**SNAPSHOT" >/dev/null 2>&1
	if [ $? -eq 0 ]; then
		echo "Previous entry is a snapshot, removing it"

		# detect first section length
		FIRST_SECTION_LENGTH=`tail -n +2 ${CHANGELOG_FILE} | nl -ba | grep -m 1 "sflphone-.* SYSTEM; urgency=.*" | awk '{print $1}'`

		if [ "$?" -ne "0" ] || [ ! ${FIRST_SECTION_LENGTH} ]; then
	        	echo " !! Error when retrieving snapshot entry length"
		        exit -1
		fi

		# remove first section
		sed -i "1,${FIRST_SECTION_LENGTH}d" ${CHANGELOG_FILE}

		if [ "$?" -ne "0" ]; then
	                echo " !! Error when removing snapshot section"
	                exit -1
        	fi
	fi

	echo -n "Generate changelog "
	IS_FIRST=1
	echo "${CHANGELOG_CONTENT}" | while read line
	do

		if [ ${IS_FIRST} ]
		then
			yes | dch --changelog ${CHANGELOG_FILE}  -b --allow-lower-version --no-auto-nmu --distribution SYSTEM --newversion ${VERSION}-SYSVER "$line" >/dev/null 2>&1
		
			if [ "$?" -ne "0" ]; then
				echo
	                	echo " !! Error with new version"
		                exit -1
	        	fi

			IS_FIRST=
		else
			dch --changelog ${CHANGELOG_FILE} --no-auto-nmu "$line"
			if [ "$?" -ne "0" ]; then
	                        echo
	                        echo " !! Error when adding changelog entry"
	                        exit -1
	                fi
		fi
		echo -n .
	done

	# add snapshot or release flag if needed
	echo
	if [ ${RELEASE_MODE} ]; then
		sed -i "3i\    ** ${VERSION} **\n" ${CHANGELOG_FILE}
                if [ "$?" -ne "0" ]; then
                        echo " !! Error when adding snapshot flag"
                        exit -1
                fi
	else
		sed -i "3i\    ** SNAPSHOT ${VERSION} **\n" ${CHANGELOG_FILE}
		if [ "$?" -ne "0" ]; then
	                echo " !! Error when adding snapshot flag"
			exit -1
		fi
	fi
	echo
done

echo "All done !"

exit 0

