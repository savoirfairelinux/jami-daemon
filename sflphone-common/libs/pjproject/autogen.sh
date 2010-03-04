#!/bin/bash
#####################################################
# File Name: autogen.sh
#
# Purpose :
#
# Author: Julien Bonjean (julien@bonjean.info) 
#
# Creation Date: 2009-05-26
# Last Modified: 2009-06-01 18:25:28 -0400
#####################################################

if [ -e /usr/share/misc/config.guess ]; then
	rm -f config.sub config.guess
        ln -s /usr/share/misc/config.sub .
        ln -s /usr/share/misc/config.guess .	
elif [ -e /usr/lib/rpm/config.guess ]; then
	rm -f config.sub config.guess
	ln -s /usr/lib/rpm/config.sub .
	ln -s /usr/lib/rpm/config.guess .
else
	aclocal --force
	automake --add-missing --force-missing --copy	
fi

exit 0
