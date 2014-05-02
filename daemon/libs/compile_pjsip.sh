#!/bin/bash

if [ "$#" -eq  "0" ]
then
    CONFIGURE=./configure
    MAKE="make dep && make"
elif [ "$1" == "-a" ]
then
    CONFIGURE=./configure-android
    # android doesn't need make
    MAKE="make dep"
elif [ "$1" == "-c" ]
then
    export CC=clang
    CONFIGURE=./configure
    MAKE="make dep && make"
else
    echo "Usage: $0 [or -a for android]"
    exit 1
fi

OPTIONS="--disable-oss
        --disable-sound
        --disable-video
        --enable-ext-sound
        --disable-speex-aec
        --disable-g711-codec
        --disable-l16-codec
        --disable-gsm-codec
        --disable-g722-codec
        --disable-g7221-codec
        --disable-speex-codec
        --disable-ilbc-codec
        --disable-opencore-amr
        --disable-sdl
        --disable-ffmpeg
        --disable-v4l2"
# TODO: autotools should be doing this
cd "`dirname $BASH_SOURCE`"/pjproject-2.2.1
CFLAGS=-g $CONFIGURE $OPTIONS && sh -c "$MAKE" && echo "pjsip successfully compiled"
