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

