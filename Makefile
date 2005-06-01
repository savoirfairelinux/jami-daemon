#
# Makefile for sflphone.
# (c) 2004 Savoir-faire Linux inc.
# Author: Laurielle Lea (laurielle.lea@savoirfairelinux.com)
#
all:
	cd stund; make
	cd src/audio/portaudio; make
	cd src/audio/gsm ; make
	cd src; make

install:
	cd src; make install

uninstall:
	cd src; make uninstall

clean:
	cd stund; make clean
	cd src/audio/portaudio; make clean
	cd src/audio/gsm ; make clean
	cd src; make clean

