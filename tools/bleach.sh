#!/bin/sh

if [ ! -e configure.ac ]
then
	echo $0 must be called from top source directory.
	exit 1
fi

if [ -e Makefile ]
then
	make distclean
fi

rm -Rf aclocal.m4 compile config.guess config.h.in config.sub configure \
  depcomp install-sh ltmain.sh Makefile.in missing stamp-h.in autom4te.cache

for mf in `find . -name Makefile.in`
do
	test -e `basename "$mf" .in`.am && rm -f "$mf"
done
