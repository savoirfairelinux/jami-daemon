#!/bin/bash

# TODO: autotools should be doing this
cd "`dirname $BASH_SOURCE`"/pjproject-2.0.1
CFLAGS="-fPIC" ./configure --disable-sound --disable-video && make dep && make -j1 && echo "pjsip successfully compiled"
