#!/bin/bash
#
# Script used by Hudson continious integration server to build SFLphone
#
# Author: Emmanuel Milou <emmanuel.milou@savoirfairelinux.com>

XML_RESULTS="cppunitresults.xml"
TEST=0
BUILD=
CODE_ANALYSIS=0

function run_code_analysis {
	# Check if cppcheck is installed on the system
	if [ `which cppcheck &>/dev/null ; echo $?` -ne 1 ] ; then
		cppcheck . --enable=all --xml 2> cppcheck-report.xml
	fi
}

function build_daemon {

	# Compile the daemon
	pushd daemon
	# Run static analysis code tool
	if [ $CODE_ANALYSIS == 1 ]; then
		run_code_analysis
	fi
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
	# Compile src code
	make -j
	# Generate documentation
	make doc
	# Compile unit tests
	make check
	popd

	if [ $TEST == 1 ]; then
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

while getopts ":b: t a" opt; do
	case $opt in
		b)
			echo "-b was triggered. Parameter: $OPTARG" >&2
			BUILD=$OPTARG
			;;
		t)
			echo "-t was triggered. Tests will be run" >&2
			TEST=1
			;;
		a)
			echo "-a was triggered. Static code analysis will be run" >&2
			CODE_ANALYSIS=1
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
build_$BUILD

# SUCCESS
exit 0
