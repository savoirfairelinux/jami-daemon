#!/bin/bash
#
# Script used by Hudson continious integration server to build SFLphone
#
# Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>

XML_RESULTS="cppunitresults.xml"

set -x
 
# Compile the daemon
pushd sflphone-common
./autogen.sh
# Compile pjproject first
pushd libs/pjproject
./autogen.sh
./configure
make && make dep
popd
./configure --prefix=/usr
make clean
make -j 1
make doc
popd

# Run the unit tests for the daemon
# pushd sflphone-common/test
# Remove the previous XML test file
# rm -rf $(XML_RESULTS)
# make check
# if at least one test failed, exit
# ./test --xml || exit 1
# popd

# Compile the client
pushd sflphone-client-gnome
./autogen.sh
./configure --prefix=/usr
make clean
make -j 1
popd

# SUCCESS
exit 0
