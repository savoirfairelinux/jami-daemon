FROM ubuntu:22.04 AS jami-daemon

ARG DEBIAN_FRONTEND=noninteractive
ARG config_args
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
    libdbus-c++-dev \
    libexpat1-dev \
    libfmt-dev \
    libgmp-dev \
    nettle-dev \
    libgnutls28-dev \
    libjsoncpp-dev \
    libmsgpack-dev \
    libnatpmp-dev \
    libopus-dev \
    libpulse-dev \
    libspeex-dev \
    libspeexdsp-dev \
    libssl-dev \
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
    yasm

# Install Node
RUN curl -fsSL https://deb.nodesource.com/setup_16.x | bash - && \
    apt-get install -y nodejs && \
    npm install -g node-gyp

# Install latest Swig (4.1)
WORKDIR /swig
RUN git clone https://github.com/swig/swig.git && \
    cd swig && \
    ./autogen.sh && \
    ./configure && \
    make -j$(nproc) && \
    make install

WORKDIR /daemon
COPY contrib/ contrib/

# Build daemon dependencies
RUN mkdir -p contrib/native && \
    cd contrib/native && \
    ../bootstrap && \
    make -j$(nproc)

COPY . .

# Build the daemon
RUN ./autogen.sh && \
    ./configure $config_args && \
    make -j$(nproc)
