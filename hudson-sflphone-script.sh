#!/bin/bash
#
# Script used by Hudson continious integration server to build SFLphone
#
# Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>

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
make
popd

# Compile the client
pushd sflphone-client-gnome
./autogen.sh
./configure --prefix=/usr
make
popd

