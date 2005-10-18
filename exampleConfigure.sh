#!/bin/sh

SFL_PREFIX=/home/$HOME/sflphone

export LD_LIBRARY_PATH=$SFL_PREFIX/lib
export LDFLAGS="-L$SFL_PREFIX/lib"
export CPPFLAGS="-I$SFL_PREFIX/include/cc++2 -I$SFL_PREFIX/include"
export PKG_CONFIG_PATH=$SFL_PREFIX/lib/pkgconfig

./configure --prefix=$SFL_PREFIX --enable-strict || exit
make || exit
make install || exit
