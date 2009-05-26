#!/bin/bash
#####################################################
# File Name: autogen.sh
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info) 
#
# Creation Date: 2009-05-26
# Last Modified: 2009-05-26 11:55:12 -0400
#####################################################

# it's only a workaround to generate config.guess and config.sub
# that are currently static
# this will generate errors but we don't care
aclocal --force >/dev/null 2>&1
automake --add-missing --force-missing --copy >/dev/null 2>&1

# now we launch configure
./configure $@

exit 0
