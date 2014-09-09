#!/bin/bash
#
# Script to build the source tarball for distribution on sflphone.org
#
# Author: Francois Marier <francois@debian.org>

# This is an environment variable provided by Jenkins. It points to the repository's root
cd ${WORKSPACE}

if [ ! -e daemon/configure.ac ] ; then
    echo "This script must be run in the root directory of the sflphone repository"
    exit 1
fi

if [ "z$1" = "z" ] ; then
    echo "Usage: $0 SOFTWARE_VERSION_NUMBER"
    exit 2
fi

# Use the version fed by launch-build-machine-jenkins.sh
SOFTWARE_VERSION=$1
BUILDDIR=sflphone-$SOFTWARE_VERSION

if [ -e $BUILDDIR ] ; then
    echo "The build directory ($BUILDDIR) already exists. Delete it first."
    exit 3
fi

mkdir $BUILDDIR
cp -r * $BUILDDIR/

pushd $BUILDDIR
rm -rf $BUILDDIR
rm -f *.tar.gz

rm -rf lang/
# No dash in Version:
sed /^Version/s/[0-9].*/${SOFTWARE_VERSION%%-*}/ tools/build-system/rpm/sflphone.spec > sflphone.spec
rm -rf tools/
#rm -rf kde/

rm -rf .git/
rm -f .gitignore

find -name .project -type f -exec rm {} \;
find -name .cproject -type f -exec rm {} \;
find -name .settings -type d -exec rm -rf {} \;

pushd daemon
./autogen.sh
find -name \*.spec -delete
popd

pushd gnome
./autogen.sh
popd

find -name autom4te.cache -type d -exec rm -rf {} \;
find -name *.in~ -type f -exec rm {} \;
popd # builddir

rm -f sflphone-*.tar.gz
tar zcf sflphone-$SOFTWARE_VERSION.tar.gz sflphone-$SOFTWARE_VERSION
rm -rf $BUILDDIR
