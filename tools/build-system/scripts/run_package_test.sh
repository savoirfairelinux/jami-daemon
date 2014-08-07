#!/bin/bash

# package_test.sh is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# package_test.sh is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with package_test.sh  If not, see <http://www.gnu.org/licenses/>.
#
# @author: Emmanuel Lepage Vallee <elv1313@gmail.com>
# @copyright: Savoir-faire Linux 2014
#
# Description:
# This script is used to quickly test packages from a CI system such as Jenkins
# by hand. It was developed in response to numerous issues with the sflphone
# ppa over the years and to quickly reproduce environment from similar to those
# used by users whose packages crashed and were reported to errors.ubuntu.com

# Settings
DEBIAN_VERSION="trusty"
IMAGEPATH="/var/chroot/"
ARCH="amd64"
IMAGENAME=${DEBIAN_VERSION}_integration.img
MASTERMOUNTPOINT=/tmp/ppa_testing/master
SNAPSHOTMOUNTPOINT=/tmp/ppa_testing/mountpoint
SNAPSHOTNAME=snapshot_$(date  '+%d.%m.%y')
IMAGE_SIZE=8         # In Gigabyte
PACKAGE_CACHE_SIZE=4 # In Gigabyte
MIRROR="http://ftp.ussg.iu.edu/linux/ubuntu/"
PACKAGES=()
REPOSITORIES=()
POST_COMMANDS=()
PRE_COMMANDS=()
PRE_UMOUNT_COMMANDS=()

# Parse arguments
OLD_IFS=$IFS
IFS=`echo -en "\n\b"`
while getopts ":a:d:p:r:s:c:b:u:m:h" opt; do
   case $opt in
   d)
      echo "Using alternate Debian derivative $OPTARG"
      DEBIAN_VERSION=$OPTARG
      ;;
   a)
      echo "Using alternate architecture $OPTARG"
      ARCH=$OPTARG
      ;;
   r)
      echo "Use alt repository (or PPA) $OPTARG"
      PPA=$OPTARG
      REPOSITORIES+=($OPTARG)
      ;;
   p)
      echo "Add package $OPTARG"
      PACKAGES+=($OPTARG)
      ;;
   p)
      MIRROR=$OPTARG
      ;;
   c)
      POST_COMMANDS+=($OPTARG)
      ;;
   b)
      echo -e "\n\n\n\nADDING " $OPTARG
      PRE_COMMANDS+=($OPTARG)
      ;;
   u)
      PRE_UMOUNT_COMMANDS+=($OPTARG)
      ;;
   s)
      echo "Open a shell before deleting the snapshot"
      OPEN_SHELL=true
      ;;
   :|h)
      echo "./create_jail.sh [-arch trusty] [-h] [-ppa link]"
      echo "a: architechture (i386, amd64, armhf)"
      echo "r: Add an apt repository to /etc/apt/sources.list (multiple allowed)"
      echo "p: Package to install (multiple allowed)"
      echo "d: alternative distribution (trusty, precise, wheezy, etc)"
      echo "c: Bash command to execute before exiting (multiple allowed)"
      echo "b: Bash command to execute before installing packages (multiple allowed)"
      echo "u: LOCAL (as ROOT on your *REAL* Linux) commands to execute before"\
         "unmounting the jail (PWD in the jail /), be careful!"
      echo "m: Ubuntu/Debian repository mirror"
      echo "shell: Open a shell before exiting"
      echo
      echo "Example usage:"
      echo "./create_jail.sh"
      exit 0
      ;;
   \?)
      echo "Invalid option: -$OPTARG", use -h for helo >&2
      exit 1
   ;;
   esac
done
IFS=$OLD_IFS

# Unmount a jail
function unmountjail() {
   CHROOT_PATH=`pwd`
   if [ "$1" != "" ]; then
      CHROOT_PATH=$1
   fi
   echo Unmounting jail on $CHROOT_PATH
   umount -l $CHROOT_PATH/dev 2> /dev/null
   umount -l $CHROOT_PATH/dev/pts 2> /dev/null
   umount -l $CHROOT_PATH/sys 2> /dev/null
   umount -l $CHROOT_PATH/proc 2> /dev/null
   umount -l $CHROOT_PATH/var/cache/apt/archives/ 2> /dev/null
}

# Mount the special APT packet cache to avoid redundant downloads
function mountcache() {
   MOUNT_PATH=$1
   CACHE_PATH=$IMAGEPATH/cache_${IMAGENAME}
   #If the cache doesn't exist, create it
   if [ ! -f $CACHE_PATH ]; then
      echo "Creating a package cache, this may take a while"
      dd if=/dev/zero of=$CACHE_PATH bs=1M count=${PACKAGE_CACHE_SIZE}000
      mkfs.btrfs $CACHE_PATH
   fi

   # Mount the cache
   mount -o loop $CACHE_PATH $MOUNT_PATH

   # Check if the cache is full, clear it
   PERCENT_USE=`df  /$MOUNT_PATH | egrep "([0-9.]+)%" -o | egrep "([0-9.]+)" -o`
   if [ $PERCENT_USE -gt 75  ]; then
      echo "The cache is full, forcing a cleanup (${PERCENT_USE} used of ${PACKAGE_CACHE_SIZE}Gb)"
      rm $MOUNT_PATH/*.deb
   fi
}

# Mount a chroot jail in the current PWD or $1
function mountjail() {
   CHROOT_PATH=`pwd`
   if [ "$1" != "" ]; then
      CHROOT_PATH=$1
   fi
   unmountjail $CHROOT_PATH
   echo Mounting jail on $CHROOT_PATH
   mount -o bind /dev $CHROOT_PATH/dev
   mount -o bind /dev/pts $CHROOT_PATH/dev/pts
   mount -o bind /sys $CHROOT_PATH/sys
   mount -o bind /proc $CHROOT_PATH/proc
   mountcache $CHROOT_PATH/var/cache/apt/archives/
}

function clearmountpoints() {
   # Close the jails
   unmountjail $SNAPSHOTMOUNTPOINT

   # Delete the snapshot
   umount -l $SNAPSHOTMOUNTPOINT 2> /dev/null
   btrfs subvolume delete ./${SNAPSHOTNAME} 2> /dev/null

   # Unmount the master
   umount -l $MASTERMOUNTPOINT 2> /dev/null
}

# Check the dependencies
if  ! command -v debootstrap ; then
   echo Please install debootstrap
   exit 1
fi
if  ! command -v btrfs ; then
   echo Please install btrfs-tools
   exit 1
fi

# Check the script can be executed
if [ "$(whoami)" != "root" ]; then
   echo This script needs to be executed as root
   exit 1
fi

# Make sure the mount points exists
mkdir $MASTERMOUNTPOINT $SNAPSHOTMOUNTPOINT $IMAGEPATH -p

cd $IMAGEPATH

# Create the container image if it doesn't already exist
if [ ! -f $IMAGENAME ]; then
   echo "Creating a disk image (use space now), this may take a while"
   dd if=/dev/zero of=$IMAGEPATH/$IMAGENAME bs=1M count=${IMAGE_SIZE}000
   mkfs.btrfs $IMAGENAME
fi

# Mount the image master snapshot
clearmountpoints
mount -o loop $IMAGEPATH/$IMAGENAME $MASTERMOUNTPOINT
cd $MASTERMOUNTPOINT

# Create the chroot if empty
if [ "$(ls)" == "" ]; then
   debootstrap --variant=buildd --arch $ARCH $DEBIAN_VERSION ./ $MIRROR #http://archive.ubuntu.com/ubuntu

   # We need universe packages
   sed -i 's/main/main universe restricted multiverse/' ./etc/apt/sources.list

   # Add the deb-src repository
   cat ./etc/apt/sources.list | sed "s/deb /deb-src /" >> ./etc/apt/sources.list
fi


# Apply updates
mountjail
chroot ./ apt-get update > /dev/null
chroot ./ apt-get upgrade -y --force-yes > /dev/null
unmountjail

# Create
btrfs subvolume snapshot . ./${SNAPSHOTNAME}

# Mount the subvolume
mount -t btrfs -o loop,subvol=${SNAPSHOTNAME} $IMAGEPATH/$IMAGENAME $SNAPSHOTMOUNTPOINT



###################################################
#                  Begin testing                  #
###################################################

mountjail $SNAPSHOTMOUNTPOINT

# Execute all PRE commands
OLD_IFS=$IFS
IFS=`echo -en "\n\b"`
for COMMAND in ${PRE_COMMANDS[@]}; do
   echo EXEC $COMMAND
   chroot $SNAPSHOTMOUNTPOINT bash -c "$COMMAND"
done
IFS=$OLD_IFS

# Add the PPA/repositories to the clear/vanilla snapshot
for REPOSITORY in ${REPOSITORIES[@]}; do
   echo deb $REPOSITORY $DEBIAN_VERSION main \
      >> $SNAPSHOTMOUNTPOINT/etc/apt/sources.list
   echo deb-src $REPOSITORY $DEBIAN_VERSION main \
      >> $SNAPSHOTMOUNTPOINT/etc/apt/sources.list
done

# Fetch/Update the repositories
chroot $SNAPSHOTMOUNTPOINT apt-get update > /dev/null 2> /dev/null

# Install each package individually
for PACKAGE in ${PACKAGES[@]}; do
   echo -e "\n\n===========Installing ${PACKAGE} ==============\n"
   chroot $SNAPSHOTMOUNTPOINT apt-get install $PACKAGE -y --force-yes
   RET=$?
   if [ "$RET" != "0" ]; then
      echo -e "\n\n\nInstall PPA to vanilla Ubuntu completed with $RET \n"
      clearmountpoints
      exit $RET
   else
      echo -e "\n\n${PACKAGE} successfully installed"
      chroot $SNAPSHOTMOUNTPOINT dpkg -s $PACKAGE
   fi
done

# Execute the post install bash commands
OLD_IFS=$IFS
IFS=`echo -en "\n\b"`
for COMMAND in ${POST_COMMANDS[@]}; do
   chroot $SNAPSHOTMOUNTPOINT /bin/bash -c "$COMMAND"
done
IFS=$OLD_IFS

# Execute commands on the *REAL OS*, as root
OLD_IFS=$IFS
IFS=`echo -en "\n\b"`
for COMMAND in ${PRE_UMOUNT_COMMANDS[@]}; do
   /bin/bash -c "cd $SNAPSHOTMOUNTPOINT;$COMMAND"
done
IFS=$OLD_IFS

# Open an interactive shell
if [ "$OPEN_SHELL" == "true" ]; then
   chroot $SNAPSHOTMOUNTPOINT /bin/bash
fi

clearmountpoints
echo "Completed successfully!"
