#!/bin/bash
#
# Script used by Hudson continious integration server to build SFLphone
#
# Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>

function build_daemon {

	# Compile the daemon
	pushd daemon
	make distclean
	./autogen.sh
	# Compile pjproject first
	pushd libs/pjproject
	./autogen.sh
	./configure
	make && make dep
	popd
	./configure --prefix=/usr
	make clean
	make -j
	make doc
	make check
	popd

	if [ $1 == 1 ]; then
		# Run the unit tests for the daemon
		pushd daemon/test
		# Remove the previous XML test file
		rm -rf $XML_RESULTS
		./run_tests.sh || exit 1
		popd
	fi
}

function build_gnome {

	# Compile the plugins
	pushd plugins
	make distclean
	./autogen.sh
	./configure --prefix=/usr
	make -j
	popd

	# Compile the client
	pushd gnome
	make distclean
	./autogen.sh
	./configure --prefix=/usr
	make clean
	make -j 1
	make check
	popd
}


if [ "$#" -eq 0 ]; then   # Script needs at least one command-line argument.
	echo "Usage $0 -b select which one to build: daemon or gnome
				  -t enable unit tests after build"
	exit $E_OPTERR
fi


git clean -f -d -x
XML_RESULTS="cppunitresults.xml"
TEST=0
BUILD=

while getopts ":b: t" opt; do
	case $opt in
		b)
			echo "-b was triggered. Parameter: $OPTARG" >&2
			BUILD=$OPTARG
			;;
		t)
			echo "-t was triggered. Tests will be run" >&2
			TEST=1
			;;
		\?)
			echo "Invalid option: -$OPTARG" >&2
			exit 1
			;;
		:)
			echo "Option -$OPTARG requires an argument." >&2
			exit 1
			;;
		esac
done

# Call appropriate build function, with parameters if needed
build_$BUILD $TEST

# SUCCESS
exit 0
