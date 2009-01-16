#!/bin/sh

# Daemon side
sudo apt-get install build-essential gnome-common \
                        libasound2-dev \
                        libpulse-dev \
                        libcommoncpp2-dev \
                        libccrtp-dev \
                        libsamplerate0-dev \
                        libdbus-1-dev \
                        libexpat1-dev \
                        libcppunit-dev \
                        libgsm1-dev \
                        sflphone-iax2-dev \
                        dbus-c++-1-dev \
                        libspeex-dev

# Client side
sudo apt-get install libgtk2.0-dev \
                        libdbus-glib-1-dev \
                        libnotify-dev \
                        libsexy-dev 

