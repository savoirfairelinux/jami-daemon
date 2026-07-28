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

# Rust toolchain, required to build the yffi (Y-CRDT) contrib package used by
# the collaborative editing feature.
ENV RUSTUP_HOME=/usr/local/rustup \
    CARGO_HOME=/usr/local/cargo \
    PATH=/usr/local/cargo/bin:/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
# The toolchain is pinned: "stable" moves under us, so an unrelated Rust release
# could break the build with no change on our side. Bump this line deliberately.
# The toolchain itself stays read-only -- a build must not be able to rewrite the
# compiler it is about to use. Only the CARGO_HOME directory is writable, which is
# all cargo needs to create its registry cache and lock file as an arbitrary user.
RUN curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs \
        | sh -s -- -y --profile minimal --default-toolchain 1.96.0 && \
    chmod -R a+rX /usr/local/rustup /usr/local/cargo && \
    chmod a+rwx /usr/local/cargo

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
