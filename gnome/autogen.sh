#!/bin/sh
# Run this to generate all the initial makefiles, etc.

test -n "$srcdir" || srcdir=`dirname "$0"`
test -n "$srcdir" || srcdir=.

OLDDIR=`pwd`
cd $srcdir

AUTORECONF=`which autoreconf`
if test -z $AUTORECONF; then
        echo "*** No autoreconf found, please install it ***"
        exit 1
fi

INTLTOOLIZE=`which intltoolize`
if test -z $INTLTOOLIZE; then
        echo "*** No intltoolize found, please install the intltool package ***"
        exit 1
fi

GNOMEDOC=`which yelp-build`
if test -z $GNOMEDOC; then
        echo "*** The tools to build the documentation are not found,"
        echo "    please install the yelp-tools package ***"
        exit 1
fi

# make sure we have gnome common, which contains macros we need
GNOMECOMMON=`which gnome-autogen.sh`
if test -z $GNOMECOMMON; then
    echo "you need to install gnome-common"
    exit 1
fi

AUTOPOINT=`which autopoint`
if test -z $AUTOPOINT; then
        echo "*** No autopoint found, please install it ***"
        exit 1
fi

autopoint --force || exit $?
AUTOPOINT='intltoolize --automake --copy' autoreconf --force --install --verbose

cd $OLDDIR
test -n "$NOCONFIGURE" || "$srcdir/configure" "$@"
