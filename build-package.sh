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

git clone git+ssh://repos-sflphone-git@sflphone.org/~/sflphone.git

# Get system parameters
arch_flag=`getconf -a|grep LONG_BIT | sed -e 's/LONG_BIT\s*//'`
os_version=`lsb_release -d -s -c | sed -e '1d'`

# If intrepid(Ubuntu8.10), then use appropriate changelog file 
if [ $os_version == "intrepid" ];then
	cp sflphone/debian/changelog.intrepid sflphone/debian/changelog
else
	cp sflphone/debian/changelog.hardy sflphone/debian/changelog
fi

# Remove useless git directory
rm sflphone/.git/ -rf

# Copy the appropriate control file based on different archtecture
if [ $arch_flag -eq 32 ] && [ $os_version == "intrepid" ];then
	cp sflphone/debian/control.intrepid.i386 sflphone/debian/control 
elif [ $arch_flag -eq 64 ] && [ $os_version == "intrepid" ];then
	cp sflphone/debian/control.intrepid.amd64 sflphone/debian/control
elif [ $arch_flag -eq 32 ] && [ $os_version == "hardy" ];then
	cp sflphone/debian/control.hardy.i386 sflphone/debian/control
else
	cp sflphone/debian/control.hardy.amd64 sflphone/debian/control
fi

echo "Building sflphone package on Ubuntu $os_version $arch_flag bit archetecture...."

# Provide prerequisite directories used by debuild
cp sflphone sflphone-0.9.2 -r
cp sflphone sflphone-0.9.2.orig -r

# Build packages
cd sflphone-0.9.2/debian; debuild

# Clean 
rm sflphone-0.9.2/ -rf 
rm sflphone/ -rf

echo "Building package finished successullly!"
