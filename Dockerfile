FROM ubuntu:24.04 AS jami-daemon

ARG DEBIAN_FRONTEND=noninteractive
ARG cmake_args
RUN apt-get update && apt-get install -y \
    autoconf \
    automake \
    autopoint \
    bison \
    build-essential \
    cmake \
    meson \
    curl \
    git \
    libarchive-dev \
    libasio-dev \
    libasound2-dev \
    libdbus-1-dev \
    libexpat1-dev \
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
    libargon2-dev \
    libwebrtc-audio-processing-dev \
    libsecp256k1-dev \
    guile-3.0-dev \
    nasm \
    pkg-config \
    yasm \
    libcppunit-dev \
    ninja-build \
    sip-tester

WORKDIR /daemon

COPY . .

# Build the daemon
RUN mkdir -p build && \
    cd build && \
    cmake .. $cmake_args -GNinja && \
    ninja -j$(($(nproc) > 4 ? 4 : $(nproc)))
