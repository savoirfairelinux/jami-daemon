#!/bin/sh

# could be replaced with autoconf -v -f (verbose, force rebuild of ltmain, .in files, etc.)
aclocal -I m4
libtoolize --force
autoheader
autoconf -f
automake -a
