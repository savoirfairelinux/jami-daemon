#
# Makefile for sflphone.
# (c) 2004 Savoir-faire Linux inc.
# Author: Laurielle Lea (laurielle.lea@savoirfairelinux.com)
#
all:
	cd gsm ; make
	cd src; make

install:
	cd src; make install

uninstall:
	cd src; make uninstall

clean:
	cd src; make clean
	cd gsm ; make clean

