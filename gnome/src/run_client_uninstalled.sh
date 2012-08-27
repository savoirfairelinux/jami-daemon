#!/bin/bash

# Usage:
#
# To run the client in debug mode:
# ./run_client_uninstalled --debug
#
# To load with proper environment in gdb:
# PROG=gdb ./run_client_uninstalled.sh
#
# same for valgrind:
# PROG="valgrind --track-origins" ./run_client_uninstalled.sh

CURRENT_DIR="`dirname $BASH_SOURCE`"
XDG_DATA_DIRS=${CURRENT_DIR}/../data:/usr/local/share:/usr/share ${PROG} ${CURRENT_DIR}/sflphone-client-gnome ${1}
