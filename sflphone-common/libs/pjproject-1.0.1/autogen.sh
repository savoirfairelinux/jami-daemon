#!/bin/bash
#####################################################
# File Name: autogen.sh
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info) 
#
# Creation Date: 2009-05-26
# Last Modified: 2009-05-27 11:00:20 -0400
#####################################################

if [ -e /usr/share/misc/config.guess ]; then
	rm -f config.sub config.guess
        ln -s /usr/share/misc/config.sub .
        ln -s /usr/share/misc/config.guess .	
else
	aclocal --force
	automake --add-missing --force-missing --copy	
fi

# now we launch configure
./configure $@

exit 0
