#!/bin/bash
#
# @author: Yun Liu <yun.liu@savoirfairelinux.com>
#
# Build sflphone debian packages for Ubuntu 8.04
#1. Before building sflphone package, you need get gpg key. Skip it if you have already one.
#   Usage: gpg --gen-key
#2. You mush have access to sflphone git repository. Skip this step if you have the access.
#   Refer to http://dev.savoirfairelinux.net/sflphone/wiki/DownloadSFLphone#Developmentsources
#3. After having all the prerequisites, you can run  "build-package.sh" to build debian packages for sflphone.
#   All the source packages and binary packages will be generated in the current directory.

if [ -d "sflphone" ]; then
        echo "Directory sflphone already exists. Please remove it first."
	exit 1
fi

# Anonymous git http access
git clone http://sflphone.org/git/sflphone.git

# Get system parameters
arch_flag=`getconf -a|grep LONG_BIT | sed -e 's/LONG_BIT\s*//'`
os_version=`lsb_release -d -s -c | sed -e '1d'`

# If intrepid(Ubuntu8.10), then use appropriate changelog file 
cp sflphone/debian/changelog.$os_version sflphone/debian/changelog

# Remove useless git directory
rm sflphone/.git/ -rf

# Copy the appropriate control file based on different archtecture
if [ $arch_flag -eq 32 ];then
	cp sflphone/debian/control.$os_version.i386 sflphone/debian/control 
elif [ $arch_flag -eq 64 ];then
	cp sflphone/debian/control.$os_version.amd64 sflphone/debian/control
fi

echo "Building sflphone package on Ubuntu $os_version $arch_flag bit architecture...."

# Provide prerequisite directories used by debuild
cp sflphone sflphone-0.9.2 -r
cp sflphone sflphone-0.9.2.orig -r

# Get the public gpg key to sign the packages
wget -q http://www.sflphone.org/downloads/gpg/sflphone.gpg.asc -O- | gpg --import -

# Build packages
cd sflphone-0.9.2/debian; debuild -k'Savoir-Faire Linux Inc.'

# Clean 
rm sflphone-0.9.2/ -rf 
rm sflphone/ -rf

echo "Building package finished successullly!"
