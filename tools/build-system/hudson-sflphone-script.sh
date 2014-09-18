#!/bin/bash -e
#
#  Copyright (C) 2004-2013 Savoir-Faire Linux Inc.
#
#  Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 3 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
#  Additional permission under GNU GPL version 3 section 7:
#
#  If you modify this program, or any covered work, by linking or
#  combining it with the OpenSSL project's OpenSSL library (or a
#  modified version of that library), containing parts covered by the
#  terms of the OpenSSL or SSLeay licenses, Savoir-Faire Linux Inc.
#  grants you additional permission to convey the resulting work.
#  Corresponding Source for a non-source form of such a combination
#  shall include the source code for the parts of OpenSSL used as well
#  as that of the covered work.
#

# Script used by Hudson continious integration server to build SFLphone

XML_RESULTS="cppunitresults.xml"
TEST=0
BUILD=
CODE_ANALYSIS=0
DOXYGEN=0
#daemon opts
DOPTS="--prefix=/usr"
#gnome opts
GOPTS="--prefix=/usr --enable-video"

#compiler defaults
export CC=gcc
export CXX=g++

CONFIGDIR=~/.config
SFLCONFDIR=${CONFIGDIR}/sflphone

function exit_clean {
    popd
    exit $1
}

function run_code_analysis {
    # Check if cppcheck is installed on the system
    if [ `which cppcheck &>/dev/null ; echo $?` -ne 1 ] ; then
        pushd src
        cppcheck . --enable=all --xml --inline-suppr 2> cppcheck-report.xml
        popd
    fi
}


function gen_doxygen {
    # Check if doxygen is installed on the system
    if [ `which doxygen &>/dev/null ; echo $?` -ne 1 ] ; then
        pushd doc/doxygen
        doxygen core-doc.cfg.in
        popd
    fi
}

function launch_functional_test_daemon {
        # Run the python functional tests for the daemon

        # make sure no other instance are currently running
        killall sflphoned
        killall sipp

        # make sure the configuration directory created
        CONFDIR=~/.config
        SFLCONFDIR=${CONFDIR}/sflphone

        eval `dbus-launch --auto-syntax`

        if [ ! -d ${CONFDIR} ]; then
            mkdir ${CONFDIR}
        fi

        if [ ! -d ${SFLCONFDIR} ]; then
            mkdir ${SFLCONFDIR}
        fi

        # make sure the most recent version of the configuration
        # is installed
        pushd tools/pysflphone
            cp -f sflphoned.functest.yml ${SFLCONFDIR}
        popd

        # launch sflphone daemon, wait some time for
        # dbus registration to complete
        pushd daemon
            ./src/sflphoned &
            sleep 3
        popd

        # launch the test script
        pushd tools/pysflphone
            nosetests --with-xunit test_sflphone_dbus_interface.py
        popd
}

function build_contrib {
    if [ -d contrib ] ; then
        pushd contrib
        mkdir -p native
        pushd native
        ../bootstrap
        # list dependencies which will be added
        make list
        make
        popd
    else
        # We're on 1.4.x
        pushd libs
        ./compile_pjsip.sh
    fi
    popd
}

function build_daemon {
    pushd daemon

    # Build dependencies first
    build_contrib

    # Run static analysis code tool
    if [ $CODE_ANALYSIS == 1 ]; then
        run_code_analysis
    fi

    # Compile the daemon
    ./autogen.sh || exit_clean 1
    #FIXME: this is a temporary hack around linking failure on jenkins
    LDFLAGS=-lgcrypt ./configure $DOPTS
    make clean
    make -j
    # Remove the previous XML test file
    rm -rf $XML_RESULTS
    # Compile unit tests
    make check
    popd
}

function build_gnome {
    # Compile the plugins
    pushd plugins
    ./autogen.sh || exit_clean 1
    ./configure $GOPTS
    make -j
    popd

    # Compile the client
    pushd gnome
    ./autogen.sh || exit_clean 1
    ./configure $GOPTS
    make clean
    make -j 1
    make check
    popd
}

function build_kde {
   # Compile the KDE client
   pushd kde
   mkdir -p build
   cd build
   cmake ../
   make -j
   popd
}


if [ "$#" -eq 0 ]; then   # Script needs at least one command-line argument.
    echo "$0 accepts the following options:
    -b select 'daemon' or 'gnome' component
    -v enable video support
    -c use clang compiler
    -a run static code analysis after build
    -t run unit tests after build"
    exit $E_OPTERR
fi

pushd "$(git rev-parse --show-toplevel)"
git clean -f -d -x

while getopts ":b: t a v c" opt; do
    case $opt in
        b)
            echo "-b is set with option $OPTARG" >&2
            if [ ! -d $OPTARG ]
            then
                echo "$OPTARG directory is missing, exiting"
                exit_clean $E_OPTERR
            fi
            BUILD=$OPTARG
            ;;
        t)
            echo "-t is set, unit tests will be run after build" >&2
            TEST=1
            ;;
        a)
            echo "-a is set, static code analysis will be run after build" >&2
            CODE_ANALYSIS=1
            ;;
        v)
            echo "-v is set, video support is disabled" >&2
            DOPTS="--disable-video $DOPTS"
            ;;
        c)
            echo "-c is set, clang compiler is used" >&2
            export CC=clang
            export CXX=clang++
            DOPTS="--without-dbus $DOPTS"
            ;;
        \?)
            echo "Invalid option: -$OPTARG" >&2
            exit_clean 1
            ;;
        :)
            echo "Option -$OPTARG requires an argument." >&2
            exit_clean 1
            ;;
        esac
done

# Call appropriate build function, with parameters if needed
build_$BUILD

if [ $TEST == 1 ]; then
    launch_functional_test_daemon
fi

# SUCCESS
exit_clean 0
