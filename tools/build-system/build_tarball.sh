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
sed /^Version/s/[0-9].*/$SOFTWARE_VERSION/ tools/build-system/rpm/sflphone.spec > sflphone.spec
rm -rf tools/
#rm -rf kde/

rm -rf .git/
rm -f .gitignore

find -name .project -type f -exec rm {} \;
find -name .cproject -type f -exec rm {} \;
find -name .settings -type d -exec rm -rf {} \;

pushd daemon
./autogen.sh
popd

pushd daemon/libs/pjproject-2.2.1
find -name os-auto.mak -type f -exec rm {} \;

# Remove pre-built binaries
rm -f pjsip/bin/pjsip-test-x86_64-unknown-linux-gnu
rm -f pjsip-apps/bin/pjsua-x86_64-unknown-linux-gnu
rm -f pjlib/bin/pjlib-test-x86_64-unknown-linux-gnu
rm -f pjnath/bin/pjnath-test-x86_64-unknown-linux-gnu
rm -f pjnath/bin/pjturn-client-x86_64-unknown-linux-gnu
rm -f pjlib-util/bin/pjlib-util-test-x86_64-unknown-linux-gnu
rm -f pjnath/bin/pjturn-srv-x86_64-unknown-linux-gnu
rm -f pjmedia/bin/pjmedia-test-x86_64-unknown-linux-gnu

rm -f third_party/portaudio/src/hostapi/asio/Pa_ASIO.pdf
rm -f third_party/portaudio/src/hostapi/asio/Callback_adaptation_.pdf

# Put actual files in the tarball
rm -f config.guess config.sub
cp /usr/share/misc/config.guess .
cp /usr/share/misc/config.sub .

rm -f config.log config.status build.mak
rm -f pjlib/include/pj/compat/m_auto.h
rm -f pjlib/include/pj/compat/os_auto.h
rm -f pjmedia/include/pjmedia-codec/config_auto.h
rm -f pjmedia/include/pjmedia/config_auto.h
rm -f pjsip/include/pjsip/sip_autoconf.h

# Remove non-GPL compatible code
sed -e 's/ milenage / /' third_party/build/Makefile > third_party/build/Makefile.new
mv third_party/build/Makefile.new third_party/build/Makefile
sed -e 's/ -lmilenage-$(TARGET_NAME) / /' build.mak.in > build.mak.in.new
mv build.mak.in.new build.mak.in
sed -e 's/ $(PJ_DIR)\/third_party\/lib\/libmilenage-$(LIB_SUFFIX) / /' build.mak.in > build.mak.in.new
mv build.mak.in.new build.mak.in
rm -rf third_party/build/milenage/
rm -rf third_party/milenage/

aclocal --force
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
