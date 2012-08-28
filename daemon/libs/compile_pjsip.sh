#!/bin/bash

# TODO: autotools should be doing this
cd "`dirname $BASH_SOURCE`"/pjproject
CFLAGS=-fPIC ./configure && make dep && make -j1 && echo "pjsip successfully compiled"
