#!/bin/sh

# could be replaced with autoconf -v -f (verbose, force rebuild of ltmain, .in files, etc.)
if [ ! -d "m4" ]; then
	mkdir m4
fi

aclocal -I m4
libtoolize --force
autoheader
autoconf -f
automake -a
#./configure $@


