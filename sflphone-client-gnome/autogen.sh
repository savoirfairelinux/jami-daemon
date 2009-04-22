#!/bin/bash

function autocmd()
{
    echo "Running ${1}..."
        $* || {
            echo "Error running ${1}"
                exit 1
        }
}

autocmd libtoolize --force --copy
autocmd aclocal
autocmd autoheader
autocmd autoconf -f
autocmd automake --add-missing --force-missing --copy -Wall

./configure $@


#!/bin/sh

# could be replaced with autoconf -v -f (verbose, force rebuild of ltmain, .in files, etc.)
#if [ ! -d "m4" ]; then
#	mkdir m4
#fi
#
#aclocal -I m4
#libtoolize --force
#autoheader
#autoconf -f
#automake -a
#./configure $@
#
#
