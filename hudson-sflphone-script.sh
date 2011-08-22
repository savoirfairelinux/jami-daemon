#!/bin/bash
#
# Script used by Hudson continious integration server to build SFLphone
#
# Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>

XML_RESULTS="cppunitresults.xml"

set -x

# Compile the daemon
pushd daemon
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
popd

# Run the unit tests for the daemon
pushd daemon/test
# Remove the previous XML test file
rm -rf $XML_RESULTS
make check
./run_tests.sh || exit 1
popd

# Compile the client
pushd gnome
./autogen.sh
./configure --prefix=/usr
make clean
make -j 1
popd

# SUCCESS
exit 0
