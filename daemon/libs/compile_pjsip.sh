#!/bin/bash

OPTIONS="--disable-oss
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
        --disable-sdl
        --disable-ffmpeg
        --disable-v4l2"
# TODO: autotools should be doing this
cd "`dirname $BASH_SOURCE`"/pjproject-2.2.1
CFLAGS=-g ./configure $OPTIONS && make dep && make -j1 && echo "pjsip successfully compiled"
