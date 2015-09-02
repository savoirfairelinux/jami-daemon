FROM debian:8

RUN apt-get update && ( apt-get upgrade -y || apt-get upgrade -y || apt-get upgrade -y )
RUN apt-get install -y devscripts debianutils debhelper build-essential tar rsync git  autoconf automake libpulse-dev libsamplerate0-dev libtool libdbus-1-dev \
 libasound2-dev libexpat1-dev libpcre3-dev libyaml-cpp-dev libboost-dev \
 libdbus-c++-dev libsndfile1-dev libsrtp-dev libxext-dev \
 libxfixes-dev yasm autopoint unbound-anchor git-core libspeex-dev \
 libspeexdsp-dev autotools-dev chrpath check astyle \
 uuid-dev libudev-dev cmake --fix-missing
