#!/bin/bash
#
# Script used by Hudson continious integration server to build SFLphone
#
# Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>

git clean -f -d
XML_RESULTS="cppunitresults.xml"

set -x

# Compile the plugins
pushd plugins
make distclean
./autogen.sh
./configure --prefix=/usr
make -j
popd

# Compile the daemon
pushd daemon
make distclean
./autogen.sh
# Compile pjproject first
pushd libs/pjproject
./autogen.sh
./configure
make && make dep
popd
./configure --prefix=/usr
make clean
make -j
make doc
make check
popd

# Run the unit tests for the daemon
pushd daemon/test
# Remove the previous XML test file
rm -rf $XML_RESULTS
./run_tests.sh || exit 1
popd

# Compile the client
pushd gnome
make distclean
./autogen.sh
./configure --prefix=/usr
make clean
make -j 1
make check
popd

# SUCCESS
exit 0
