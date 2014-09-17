#!/bin/bash
#
# Script to build the source tarball for distribution on sflphone.org
# Inclusion of KDE is a requirement. Run get-kde.sh to have it.
#
# Author: Francois Marier <francois@debian.org>


# Exit on error
set -o errexit

# This is an environment variable provided by Jenkins. It points to the repository's root
cd ${WORKSPACE}

if [ ! -e daemon/configure.ac ] ; then
    echo "This script must be run in the root directory of the sflphone repository" >&2
    exit 1
fi

if [ $# -ne 1 ] ; then
    echo "Usage: $(basename $0) SOFTWARE_VERSION_NUMBER" >&2
    exit 2
fi

if [ ! -d kde ] ; then
    echo 'No "kde" directory. Make sure get-kde.sh ran at some point.' >&2
fi

# Use the version fed by launch-build-machine-jenkins.sh
SOFTWARE_VERSION=$1
BUILDDIR=sflphone-$SOFTWARE_VERSION

if [ -e $BUILDDIR ] ; then
    echo "The build directory ($BUILDDIR) already exists. Delete it first." >&2
    exit 3
fi

# Populate the tarball directory
mkdir $BUILDDIR
SRCITEMS=$(echo *)
# Exclude existing tarballs from the created tarball
SRCITEMS=${SRCITEMS//*.tar.gz}
# ${SRCITEMS//$BUILDDIR} is used to remove $BUILDDIR from $SRCITEMS
# See bash parameter expansion
cp -r ${SRCITEMS//$BUILDDIR} $BUILDDIR/

pushd $BUILDDIR
# No dash in Version:
sed /^Version/s/[0-9].*/${SOFTWARE_VERSION%%-*}/ tools/build-system/rpm/sflphone.spec > sflphone.spec

# Remove unwanted files
rm -rf lang/
rm -rf tools/
rm -rf .git/
find -name .gitignore -delete
find -name .project -type f -delete
find -name .cproject -type f -delete
find -name .settings -type d -exec rm -rf {} +

# Generate the configure files
pushd daemon
./autogen.sh
find -name \*.spec -delete
popd

pushd gnome
./autogen.sh
popd

find -name autom4te.cache -type d -exec rm -rf {} +
find -name *.in~ -type f -delete
popd # builddir

tar zcf sflphone-${SOFTWARE_VERSION}.tar.gz sflphone-${SOFTWARE_VERSION}
rm -rf $BUILDDIR
