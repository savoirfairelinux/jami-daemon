#!/bin/bash
#####################################################
# File Name: sfl-git-dch.sh
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info)
#
# Creation Date: 2009-10-21
# Last Modified: 2009-10-21 14:58:22 -0400
#####################################################

#set -x

. $1

echo "********************************************************************************"
echo "Software: ${SOFTWARE}"
echo "Version: ${VERSION}"
echo "Distribution: ${DISTRIBUTION}"
echo "Generating changelog (from commit ${COMMIT_HASH_BEGIN} to ${COMMIT_HASH_END}) in file ${CHANGELOG_FILE}"
if [ ${IS_RELEASE} ] ; then
	echo "Release mode"
else
	echo "Snapshot mode"
fi

cd ${WORKING_DIR}

# use git log to retrieve changelog content
CHANGELOG_CONTENT=`git log --no-merges --pretty=format:"%s" ${COMMIT_HASH_BEGIN}..${COMMIT_HASH_END} $2 | grep -v "\[\#1262\]"`

if [ "$?" -eq "1" ]; then
        echo " !! No new commit since last release"
	CHANGELOG_CONTENT="No new commit"
fi

if [ "$?" -ne "0" ]; then
        echo " !! Error when retrieving changelog content"
        exit -1
fi

rm -f ${CHANGELOG_FILE}.dch >/dev/null 2>&1	

IS_FIRST=1
echo "${CHANGELOG_CONTENT}" | while read line
do
	if [ ${IS_FIRST} ]; then

		yes | dch --changelog ${CHANGELOG_FILE}  -b --allow-lower-version --no-auto-nmu --distribution ${DISTRIBUTION} --newversion ${VERSION} "$line" >/dev/null 2>&1
		
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
if [ ${IS_RELEASE} ]; then
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
echo "All done !"
echo "********************************************************************************"

cd -

exit 0

