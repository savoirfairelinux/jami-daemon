#!/bin/sh

if [ ! -e configure.ac ]
then
	echo $0 must be called from top source directory.
	exit 1
fi

echo autoreconf ...
echo > stamp-h.in
autoreconf --verbose --force --install --warnings=all,no-portability || { echo failed. ; exit 1 ; }

