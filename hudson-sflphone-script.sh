#!/bin/bash
#
# Script used by Hudson continious integration server to build SFLphone
#
# Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>

pushd sflphone-common
./autogen.sh
./configure --prefix=/usr
make
popd

pushd sflphone-client-gnome
./autogen.sh
./configure --prefix=/usr
make
popd

