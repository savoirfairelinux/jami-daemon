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
autocmd automake --add-missing --force-missing --copy -Wall
autocmd autoconf

echo "Autogen done, now you can ./configure"
