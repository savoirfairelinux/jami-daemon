#!/bin/sh -e

# Workaround for http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=565663
[ ! -e m4 ] && mkdir m4

command -v gnome-doc-prepare >/dev/null 2>&1 || { echo >&2 "Please install gnome-doc-utils.  Aborting."; exit 1; }

gnome-doc-prepare --force
autoreconf --force --install --verbose -Wall -I m4
