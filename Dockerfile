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

RUN curl -fsSL https://deb.nodesource.com/setup_26.x | bash - && \
    apt-get install -y nodejs && \
    node --version && \
    npm --version

WORKDIR /daemon

COPY . .

# Install SWIG 4.3+
RUN apt-get update && apt-get install -y \
    wget \
    pcre2-utils \
    libpcre2-dev \
    && \
    wget https://github.com/swig/swig/archive/refs/tags/v4.3.1.tar.gz -O /tmp/swig.tar.gz && \
    tar -xzf /tmp/swig.tar.gz -C /tmp && \
    cd /tmp/swig-4.3.1 && \
    ./autogen.sh && \
    ./configure && \
    make -j$(nproc) && \
    make install && \
    rm -rf /tmp/swig*

# Install the pinned Rust toolchain used by the daemon contrib build.
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | \
    sh -s -- -y --profile minimal --default-toolchain 1.96.0

ENV PATH="/root/.cargo/bin:${PATH}"

RUN rustc --version && \
    cargo --version

RUN cd /daemon/bin/nodejs && \
    npm install --no-save node-addon-api

# Build the daemon
RUN mkdir -p build && \
    cd build && \
    cmake .. $cmake_args -GNinja && \
    ninja -j$(($(nproc) > 4 ? 4 : $(nproc)))
