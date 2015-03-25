#!/bin/bash

AUTORECONF=`which autoreconf`
if test -z $AUTORECONF; then
        echo "*** No autoreconf found, please install it ***"
        exit 1
fi

if !"${PKG_CONFIG:-pkg-config}" --version >/dev/null 2>&1; then
    echo "*** No pkg-config found, please install it ***"
    # warn without exiting, since pkg-config is only needed
    # by configure, not autogen.sh
fi

LIBTOOLIZE=`which libtoolize || which glibtoolize`
if test -z $LIBTOOLIZE; then
    LIBTOOLIZE=`which glibtoolize`
    if test -z $LIBTOOLIZE; then
        echo "*** No libtool found, please install it ***"
        exit 1
    fi
fi

# Workaround for http://bugs.debian.org/cgi-bin/bugreport.cgi?bug=565663
mkdir -p m4

HOOKS_DIR=".git/hooks"
# install pre-commit hook for doing clean commits
if [ -d "$HOOKS_DIR" ];
then
    pushd ${HOOKS_DIR}
    if test ! \( -x pre-commit -a -L pre-commit \);
    then
        echo "Creating symbolic link for pre-commit hook..."
        rm -f pre-commit
        ln -s pre-commit.sample pre-commit
    fi
    popd
fi

autoreconf --force --install --verbose -Wall -I m4
