FROM ubuntu:15.10

RUN apt-get update && ( apt-get upgrade -y || apt-get upgrade -y || apt-get upgrade -y )
RUN apt-get install -y devscripts debianutils debhelper build-essential tar rsync git \
 autoconf automake libpulse-dev libsamplerate0-dev libtool libdbus-1-dev \
 libasound2-dev libexpat1-dev libpcre3-dev libyaml-cpp-dev libboost-dev \
 libdbus-c++-dev libsndfile1-dev libsrtp-dev libxext-dev libgnutls-dev \
 libxfixes-dev yasm autopoint unbound-anchor git-core libspeex-dev \
 libspeexdsp-dev autotools-dev chrpath check astyle libupnp-dev qtbase5-dev \
 libgnutls28-dev libebook1.2-dev gnome-icon-theme-symbolic libclutter-gtk-1.0-dev \
 libclutter-1.0-dev libgtk-3-dev libnotify-dev qttools5-dev-tools qttools5-dev \
 uuid-dev libudev-dev cmake --fix-missing
