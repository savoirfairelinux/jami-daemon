#!/bin/bash
#####################################################
# File Name: launch-build-machine.sh
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info) 
#
# Creation Date: 2009-04-20
# Last Modified: 2009-06-09 17:51:40 -0400
#####################################################

#
# Not working with git 1.5.4.3
#

TAG=`date +%Y-%m-%d`

# wait delay after startup and shutdown of VMs
STARTUP_WAIT=40
SHUTDOWN_WAIT=30

# ssh stuff
SSH_OPTIONS="-o LogLevel=ERROR -o CheckHostIP=no -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null"
SSH_HOST="sflphone@127.0.0.1"
SSH_REPOSITORY_HOST="sflphone-package-manager@dev.savoirfairelinux.net"
SSH_BASE="ssh ${SSH_OPTIONS} -p 50001 ${SSH_HOST}"
SCP_BASE="scp ${SSH_OPTIONS} -r -P 50001"

# home directory
ROOT_DIR="/home/projects/sflphone"

# vbox config directory
export VBOX_USER_HOME="${ROOT_DIR}/vbox"

# remote home directory
REMOTE_ROOT_DIR="/home/sflphone"

# scripts
SCRIPTS_DIR="${ROOT_DIR}/build-system"
PACKAGING_SCRIPTS_DIR="${SCRIPTS_DIR}/remote"
DISTRIBUTION_SCRIPTS_DIR="${SCRIPTS_DIR}/distributions"

# directory that will be deployed to remote machine
TODEPLOY_DIR="${ROOT_DIR}/sflphone-packaging"
TODEPLOY_BUILD_DIR="${TODEPLOY_DIR}/build"

# remote deployment dir
REMOTE_DEPLOY_DIR="/home/sflphone/sflphone-packaging"

# cloned repository and archive
REPOSITORY_DIR="${TODEPLOY_BUILD_DIR}/sflphone"
REPOSITORY_ARCHIVE="`dirname ${REPOSITORY_DIR}`/sflphone.tar.gz"
REPOSITORY_SFLPHONE_COMMON_DIR="${REPOSITORY_DIR}/sflphone-common"
REPOSITORY_SFLPHONE_CLIENT_KDE_DIR="${REPOSITORY_DIR}/sflphone-client-kde"
REPOSITORY_SFLPHONE_CLIENT_GNOME_DIR="${REPOSITORY_DIR}/sflphone-client-gnome"

# where results go
PACKAGING_RESULT_DIR=${ROOT_DIR}/packages-${TAG}

USER="sflphone"

RELEASE_MODE=

SNAPSHOT_TAG=`date +%s`

DO_PREPARE=1
DO_PUSH=1
DO_MAIN_LOOP=1
DO_SIGNATURES=1
DO_UPLOAD=1
DO_LOGGING=1
DO_SEND_EMAIL=1

EDITOR=echo
export EDITOR

NON_FATAL_ERRORS=0

MACHINES=( "ubuntu-8.04" "ubuntu-8.04-64" "ubuntu-8.10" "ubuntu-8.10-64" "ubuntu-9.04" "ubuntu-9.04-64" "opensuse-11" "opensuse-11-64" "mandriva-2009.1" )

#########################
# BEGIN
#########################

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
		echo " --skip-prepare"
		echo " --skip-push"
		echo " --skip-main-loop"
		echo " --skip-signatures"
		echo " --skip-upload"
		echo " --no-logging"
		echo " --machine=MACHINE"
		echo " --release-mode=[beta|rc|release]"
		echo " --list-machines"
		echo
		exit 0;;
	--skip-prepare)
		unset DO_PREPARE;;
	--skip-push)
		unset DO_PUSH;;
	--skip-main-loop)
		unset DO_MAIN_LOOP;;
	--skip-signatures)
		unset DO_SIGNATURES;;
	--skip-upload)
		unset DO_UPLOAD;;
	--no-logging)
		unset DO_LOGGING;;
	--machine=*)
		MACHINES=(${PARAMETER##*=});;
	--release-mode=*)
		RELEASE_MODE=(${PARAMETER##*=});;
	--list-machines)
		echo "Available machines :"
		for MACHINE in ${MACHINES[@]}; do
			echo " "${MACHINE}
		done
		exit 0;;
	*)
		echo "Unknown parameter : ${PARAMETER}"
		exit -1;;
	esac
done

# if more than one VM will be launched, automatically stop running VMs
if [ "${#MACHINES[@]}" -gt "1" ]; then
	VBoxManage list runningvms | tail -n +5 | awk '{print $1}' | xargs -i VBoxManage controlvm {} poweroff
fi

# change to working directory
cd ${SCRIPTS_DIR}

if [ "$?" -ne "0" ]; then
        echo " !! Cannot cd to working directory"
        exit -1
fi

WHO=`whoami`

if [ "${WHO}" != "${USER}" ]; then
        echo "!! Please use user ${USER} to run this script"
        exit -1;
fi

# logging
rm -rf ${PACKAGING_RESULT_DIR} 2>/dev/null
mkdir ${PACKAGING_RESULT_DIR} 2>/dev/null
if [ ${DO_LOGGING} ]; then

	# open file descriptor
	rm -f ${PACKAGING_RESULT_DIR}/packaging.log
	exec 3<> ${PACKAGING_RESULT_DIR}/packaging.log

	# redirect outputs (stdout & stderr)
	exec 1>&3
	exec 2>&3
fi

# check release
if [ ${RELEASE_MODE} ]; then
	case ${RELEASE_MODE} in
		beta);;
		rc[1-9]);;
		release);;
		*)
			echo "Bad release mode"
			exit -1;;
	esac
fi

# check machines list
if [ -z "${MACHINES}" ]; then
	echo "Need at least a machine name to launch"
	exit -1
fi

echo
echo "Launching build system with the following machines :"
for MACHINE in ${MACHINES[*]}
do
	echo " "${MACHINE}
done
echo

if [ ${RELEASE_MODE} ]; then
	echo "Release mode : ${RELEASE_MODE}"
else
	echo "Snapshot mode : ${SNAPSHOT_TAG}"
fi

#########################
# COMMON PART
#########################

if [ ${DO_PREPARE} ]; then

	echo
	echo "Cleaning old deploy dir"
	rm -rf ${TODEPLOY_DIR}
	mkdir ${TODEPLOY_DIR}
	mkdir ${TODEPLOY_BUILD_DIR}

	echo "Clone repository"
	git clone ssh://repos-sflphone-git@sflphone.org/~/sflphone.git ${REPOSITORY_DIR} >/dev/null 2>&1

	if [ "$?" -ne "0" ]; then
		echo " !! Cannot clone repository"
		exit -1
	fi

	VERSION=`cd ${REPOSITORY_DIR} && git describe --tag HEAD  | cut -d "/" -f2 | cut -d "-" -f1`

	if [ ${RELEASE_MODE} ]; then
		if [ "${RELEASE_MODE}" != "release" ];then
			VERSION="${VERSION}~${RELEASE_MODE}"
		fi
	else
		VERSION="${VERSION}~snapshot${SNAPSHOT_TAG}"
	fi
	echo "Version is : ${VERSION}"

	# if push is activated
	if [ ${DO_PUSH} ];then

		# first changelog generation for commit
		echo "Update debian changelogs (1/2)"

		${SCRIPTS_DIR}/sfl-git-dch.sh ${VERSION} ${RELEASE_MODE}

		if [ "$?" -ne "0" ]; then
			echo "!! Cannot update debian changelogs"
			exit -1
		fi

		echo " Doing commit"
		
        	cd ${REPOSITORY_DIR}
		git commit -m "[#1262] Updated debian changelogs (${VERSION})" . >/dev/null

		echo " Pushing commit"
		git push origin master >/dev/null
	fi

	# change current branch if needed
        if [ ${RELEASE_MODE} ]; then
                cd ${REPOSITORY_DIR}
                git checkout origin/release -b release
        else
                echo "Using master branch"
        fi

	# generate the changelog, according to the distribution and the git commit messages
	echo "Update debian changelogs (2/2)"
	cd ${REPOSITORY_DIR}
	${SCRIPTS_DIR}/sfl-git-dch.sh ${VERSION} ${RELEASE_MODE}
	
	if [ "$?" -ne "0" ]; then
		echo "!! Cannot update debian changelogs"
		exit -1
	fi

	echo "Write version numbers for following processes"
	echo "${VERSION}" > ${REPOSITORY_DIR}/sflphone-common/VERSION
	echo "${VERSION}" > ${REPOSITORY_DIR}/sflphone-client-gnome/VERSION
	echo "${VERSION}" > ${REPOSITORY_DIR}/sflphone-client-kde/VERSION
	echo "${VERSION}" > ${TODEPLOY_BUILD_DIR}/VERSION

	echo "Archiving repository"
	tar czf ${REPOSITORY_ARCHIVE} --exclude .git -C `dirname ${REPOSITORY_DIR}` sflphone 

	if [ "$?" -ne "0" ]; then
		echo " !! Cannot archive repository"
		exit -1
	fi

	echo  "Removing repository"
	rm -rf ${REPOSITORY_DIR}

	echo "Finish preparing deploy directory"
	cp -r ${DISTRIBUTION_SCRIPTS_DIR}/* ${TODEPLOY_DIR}

	if [ "$?" -ne "0" ]; then
		echo " !! Cannot prepare scripts for deployment"
		exit -1
	fi
fi

#########################
# MAIN LOOP
#########################

if [ ${DO_MAIN_LOOP} ]; then

	echo
	echo "Entering main loop"
	echo

	rm -f ${PACKAGING_RESULT_DIR}/stats.log
	for MACHINE in ${MACHINES[*]}
	do

		echo "Launch machine ${MACHINE}"
		VM_STATE=`VBoxManage showvminfo ${MACHINE} | grep State | awk '{print $2}'`
		if [ "${VM_STATE}" = "running" ]; then
			echo "Not needed, already running"
		else
			cd ${VBOX_USER_HOME} && VBoxHeadless -startvm "${MACHINE}" -p 50000 &
			if [[ ${MACHINE} =~ "opensuse" ]]; then
				STARTUP_WAIT=200
			fi
			echo "Wait ${STARTUP_WAIT} s"
			sleep ${STARTUP_WAIT}
		fi
	
		echo "Clean remote directory"
		${SSH_BASE} "rm -rf ${REMOTE_DEPLOY_DIR} 2>/dev/null"

		echo "Deploy packaging system"
		${SCP_BASE} ${TODEPLOY_DIR} ${SSH_HOST}:

		if [ "$?" -ne "0" ]; then
	                echo " !! Cannot deploy packaging system"
			echo "${MACHINE} : Cannot deploy packaging system" >> ${PACKAGING_RESULT_DIR}/stats.log
			NON_FATAL_ERRORS=1
	        else

			echo "Launch remote build"
			${SSH_BASE} "cd ${REMOTE_DEPLOY_DIR} && ./build-packages.sh ${RELEASE_MODE}"

			if [ "$?" -ne "0" ]; then
	                	echo " !! Error during remote packaging process"
				echo "${MACHINE} : Error during remote packaging process" >> ${PACKAGING_RESULT_DIR}/stats.log
				NON_FATAL_ERRORS=1
	        	else

				echo "Retrieve dists files"
				${SCP_BASE} ${SSH_HOST}:${REMOTE_DEPLOY_DIR}/deb ${PACKAGING_RESULT_DIR}/ >/dev/null 2>&1
				${SCP_BASE} ${SSH_HOST}:${REMOTE_DEPLOY_DIR}/rpm ${PACKAGING_RESULT_DIR}/ >/dev/null 2>&1
				
				echo "${MACHINE} : OK" >> ${PACKAGING_RESULT_DIR}/stats.log
			fi

			echo "Retrieve log files"
			${SCP_BASE} ${SSH_HOST}:${REMOTE_DEPLOY_DIR}"/*.log" ${PACKAGING_RESULT_DIR}/
		fi

		if [ "${VM_STATE}" = "running" ]; then
			echo "Leave machine running"
		else
			echo "Shut down machine ${MACHINE}"
			${SSH_BASE} 'sudo /sbin/shutdown -h now'
			echo "Wait ${SHUTDOWN_WAIT} s"
			sleep ${SHUTDOWN_WAIT}
			# hard shut down (just to be sure)
			cd "${VBOX_USER_HOME}" && VBoxManage controlvm ${MACHINE} poweroff >/dev/null 2>&1
		fi
	done
fi

#########################
# SIGNATURES
#########################

if [ ${DO_SIGNATURES} ]; then
	
	echo
	echo "Sign packages"
	echo

	echo  "Check GPG agent"
	pgrep -u "sflphone" gpg-agent > /dev/null
	if [ "$?" -ne "0" ]; then
	        echo "!! GPG agent is not running"
		exit -1
	fi
	GPG_AGENT_INFO=`cat $HOME/.gpg-agent-info 2> /dev/null`
	export ${GPG_AGENT_INFO}

	if [ "${GPG_AGENT_INFO}" == "" ]; then
        	echo "!! Cannot get GPG agent info"
	        exit -1
	fi	

	echo "Sign packages"
	find ${PACKAGING_RESULT_DIR}/deb/dists -name "*.deb" -exec dpkg-sig -k 'Savoir-Faire Linux Inc.' --sign builder --sign-changes full {} \; >/dev/null 2>&1
	find ${PACKAGING_RESULT_DIR}/deb/dists -name "*.changes" -printf "debsign -k'Savoir-Faire Linux Inc.' %p\n" | sh >/dev/null 2>&1
fi

#########################
# UPLOAD FILES
#########################

if [ ${DO_UPLOAD} ]; then
	
	echo
	echo "Upload packages"
	echo

	echo "Prepare packages upload"
	scp ${SSH_OPTIONS} ${PACKAGING_SCRIPTS_DIR}/update-repository.sh ${SSH_REPOSITORY_HOST}:debian/ 

	if [ "$?" -ne "0" ]; then
                echo " !! Cannot deploy repository scripts"
        fi
	
	echo "Upload packages"
	echo "Install dists files to repository"
	scp -r ${SSH_OPTIONS} ${PACKAGING_RESULT_DIR}/rpm/* ${SSH_REPOSITORY_HOST}:rpm/
	scp -r ${SSH_OPTIONS} ${PACKAGING_RESULT_DIR}/deb/dists ${SSH_REPOSITORY_HOST}:debian/

	if [ "$?" -ne "0" ]; then
		echo " !! Cannot upload packages"
		exit -1
	fi

	echo "Update repository"
	ssh ${SSH_OPTIONS} ${SSH_REPOSITORY_HOST} "cd debian && ./update-repository.sh"

	if [ "$?" -ne "0" ]; then
		echo " !! Cannot update repository"
		exit -1
	fi
fi

if [ "${NON_FATAL_ERRORS}" -eq "1" ]; then
	exit -1
fi

# close file descriptor
exec 3>&-

exit 0

