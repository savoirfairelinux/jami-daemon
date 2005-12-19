#!/bin/sh

SFL_PREFIX=$HOME/sflphone
OSIP_PREFIX=$SFL_PREFIX

export LD_LIBRARY_PATH=$SFL_PREFIX/lib
export LDFLAGS="-L$SFL_PREFIX/lib"
export CPPFLAGS="-I$SFL_PREFIX/include/cc++2 -I$SFL_PREFIX/include"
export PKG_CONFIG_PATH=$SFL_PREFIX/lib/pkgconfig

./configure --prefix=$SFL_PREFIX --enable-strict --with-osip=$OSIP_PREFIX || exit
make || exit
make install || exit
