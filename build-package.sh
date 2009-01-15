#!/bin/bash
#
# @author: Yun Liu <yun.liu@savoirfairelinux.com>
#
# Build sflphone debian packages for Ubuntu 8.04
# 1 - The SFLphone package must be build with a specific GnuPG key. Please contact us to have more information about that (<sflphoneteam@savoirfairelinux.com>) 
# 2. The source code can be teched through anonymous http access. So no need of special access.
# 3. After having all the prerequisites, you can run  "build-package.sh" to build debian packages for sflphone.
#   All the source packages and binary packages will be generated in the current directory.
# Refer to http://www.sflphone.org for futher information

if [ -d "sflphone" ]; then
        echo "Directory sflphone already exists. Please remove it first."
	exit 1
fi

# Anonymous git http access
git clone http://sflphone.org/git/sflphone.git
cd sflphone
git checkout origin/release release

# Get system parameters
arch_flag=`getconf -a|grep LONG_BIT | sed -e 's/LONG_BIT\s*//'`
os_version=`lsb_release -d -s -c | sed -e '1d'`

# Generate the changelog, according to the distribution and the git commit messages
git-dch --debian-branch=release --release
cd ..

# Remove useless git directory
rm sflphone/.git/ -rf

# Copy the appropriate control file based on different archtecture
cp sflphone/debian/control.$os_version sflphone/debian/control

echo "Building sflphone package on Ubuntu $os_version $arch_flag bit architecture...."

# Provide prerequisite directories used by debuild
cp sflphone sflphone-0.9.2 -r
cp sflphone sflphone-0.9.2.orig -r

# Get the public gpg key to sign the packages
wget -q http://www.sflphone.org/downloads/gpg/sflphone.gpg.asc -O- | gpg --import -

# Build packages
cd sflphone-0.9.2/debian; debuild -k'Savoir-Faire Linux Inc.'

# Clean 
cd ../..
rm sflphone-0.9.2/ -rf 
rm sflphone/ -rf

echo "Building package finished successullly!"
