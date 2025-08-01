FROM ubuntu:22.04 AS jami-daemon

ARG DEBIAN_FRONTEND=noninteractive
ARG cmake_args
RUN apt-get update && apt-get install -y \
    autoconf \
    automake \
    autopoint \
    bison \
    build-essential \
    cmake \
    curl \
    git \
    libarchive-dev \
    libasio-dev \
    libasound2-dev \
    libdbus-1-dev \
    libexpat1-dev \
    libfmt-dev \
    libgmp-dev \
    nettle-dev \
    libgnutls28-dev \
    libjsoncpp-dev \
    libmsgpack-dev \
    libnatpmp-dev \
    libopus-dev \
    libpipewire-0.3-dev \
    libpulse-dev \
    libspeex-dev \
    libspeexdsp-dev \
    libssl-dev \
    libsystemd-dev \
    libtool \
    libudev-dev \
    libupnp-dev \
    libva-dev \
    libvdpau-dev \
    libvpx-dev \
    libx264-dev \
    libyaml-cpp-dev \
    libhttp-parser-dev \
    libwebrtc-audio-processing-dev \
    libsecp256k1-dev \
    guile-3.0-dev \
    nasm \
    pkg-config \
    yasm \
    libcppunit-dev \
    sip-tester

# Install Node
RUN curl -fsSL https://deb.nodesource.com/setup_22.x | bash - && \
    apt-get install -y nodejs && \
    npm install -g node-gyp

# Install latest Swig (4.2)
WORKDIR /swig
RUN git clone https://github.com/swig/swig.git && \
    cd swig && \
    ./autogen.sh && \
    ./configure && \
    make -j$(nproc) && \
    make install

WORKDIR /daemon

COPY . .

# Build the daemon
RUN mkdir -p build && \
    cd build && \
    cmake .. $cmake_args && \
    make -j$(nproc)
