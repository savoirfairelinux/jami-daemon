FROM ubuntu:24.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive

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
    libasio-dev \
    libasound2-dev \
    libdbus-1-dev \
    libsystemd-dev \
    libva-dev \
    libvdpau-dev \
    libtool \
    libudev-dev \
    libpcre2-dev \
    nasm \
    pkg-config \
    yasm \
    ninja-build

# Install Node
RUN curl -fsSL https://deb.nodesource.com/setup_24.x | bash - && \
    apt-get install -y nodejs && \
    npm install -g node-gyp node-addon-api
ENV NODE_PATH=/usr/lib/node_modules

# Install latest Swig (>= 4.3 required for N-API support)
WORKDIR /swig
RUN git clone https://github.com/swig/swig.git --depth 1 && \
    cd swig && \
    ./autogen.sh && \
    ./configure && \
    make -j$(nproc) && \
    make install

WORKDIR /daemon

COPY . .

# Build the daemon
ENV DISABLE_PIPEWIRE=1
ENV DISABLE_LINUX_HWACCEL=1
ENV IGNORE_SYSTEM_LIBS=1
RUN mkdir -p build && \
    cd build && \
    cmake .. -DBUILD_TESTING=Off -DCMAKE_BUILD_TYPE=Release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON -DJAMI_NODEJS=ON -GNinja && \
    ninja -j$(($(nproc) > 4 ? 4 : $(nproc)))
