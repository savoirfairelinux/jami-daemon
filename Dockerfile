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
    libasound2-dev \
    libdbus-1-dev \
    libexpat1-dev \
    libpulse-dev \
    libtool \
    libudev-dev \
	libva-dev \
    libvdpau-dev \
    nasm \
    pkg-config \
	yasm

# Install Node
RUN curl -fsSL https://deb.nodesource.com/setup_16.x | bash - && \
	apt-get install -y nodejs && \
    npm install -g node-gyp

# Install latest Swig (4.1)
WORKDIR /swig
RUN	git clone https://github.com/swig/swig.git && \
    cd swig && \
    ./autogen.sh && \
    ./configure && \
    make -j$(nproc) && \
    make install

WORKDIR /daemon
COPY . .

# Build daemon dependencies
RUN mkdir -p contrib/native && \
    cd contrib/native && \
    ../bootstrap && \
    make -j$(nproc)

# Build the daemon
RUN ./autogen.sh && \
    ./configure $config_args && \
    make -j$(nproc)
