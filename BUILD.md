# Building the daemon

This document provides instructions on how to build the Jami daemon from source code. Ensure that you have met all the dependencies before proceeding with the build.

## Dependencies

### Core Required Dependencies

**Build System:**
- CMake or Meson
- C++17 compiler
- pkg-config

**Core Libraries:**
- **opendht** - Distributed hash table
- **dhtnet** - Network utilities for DHT
- **gnutls** - TLS library
- **nettle** - Cryptographic library
- **libpjproject** (custom fork) - SIP stack
- **libgit2** - Git library
- **libsecp256k1** - Elliptic curve cryptography
- **fmt** - Formatting library
- **yaml-cpp** - YAML parser
- **jsoncpp** - JSON library
- **simdutf** - SIMD UTF validation
- **zlib** - Compression library

**FFmpeg Libraries:**
- libavcodec
- libavfilter
- libavdevice
- libavformat
- libswscale
- libswresample
- libavutil

### Platform-Specific Dependencies

**Linux:**
- libudev
- ALSA - optional
- PulseAudio - optional

**Android:**
- Android NDK
- OpenSLES

**macOS:**
- Package manager tools (Homebrew or MacPorts recommended)
- automake, pkg-config, libtool, gettext, yasm

**Windows:**
- pywinmake

### Optional Dependencies

- **sdbus-c++** - for D-Bus interface
- **webrtc-audio-processing** - for WebRTC audio processing
- **speexdsp** - for Speex audio processing
- **jack** - JACK audio server
- **portaudio** - PortAudio
- **libarchive** / **minizip** - for plugin support
- **cppunit** - for tests
- **swig** - for Node.js bindings
- **node-gyp** - for Node.js bindings
- **openssl** - SSL/TLS library

### Contrib Dependencies

The project includes a contrib system that can build many dependencies from source. The contrib/src directory contains source packages for approximately 50+ libraries including argon2, asio, ffmpeg, opencv, opus, and many others. When using CMake or the bootstrap script, these dependencies will be built automatically.

----

## Build Instructions

A) With CMake

```bash
mkdir build
cd build
cmake .. -DJAMI_DBUS=On
make -j4
```

This should build the 'contrib' dependencies, then the daemon

B) With Meson

1) Compile the dependencies first

```bash
cd contrib
mkdir native
cd native
../bootstrap
make
```

2) Then the jamid application and/or libjami library

```bash
cd ../../
mkdir build
export PATH=$PATH:`pwd`/contrib/`cc -dumpmachine`/bin
meson -Dpkg_config_path=`pwd`/contrib/`cc -dumpmachine`/lib/pkgconfig -Ddefault_library=static -Dinterfaces=dbus build
cd build
ninja
ninja install
```

How to compile the daemon for Android (on a Linux or macOS host)
----

A) With CMake

```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_API=24 -DBUILD_EXTRA_TOOLS=On -DJAMI_JNI=On -DJAMI_JNI_PACKAGEDIR=java
make -j4
```

Replace arm64-v8a with the desired target ABI.
See the README in jami-client-android for instructions to build the Jami client for Android.

B) With Meson

1) Download and install Android NDK
2) Compile the dependencies

```bash
export ANDROID_NDK=<NDK>
export ANDROID_ABI=arm64-v8a
export ANDROID_API=24
export TOOLCHAIN=$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64
export TARGET=aarch64-linux-android
export CC=$TOOLCHAIN/bin/$TARGET$ANDROID_API-clang
export CXX=$TOOLCHAIN/bin/$TARGET$ANDROID_API-clang++
export AR=$TOOLCHAIN/bin/$TARGET-ar
export LD=$TOOLCHAIN/bin/$TARGET-ld
export RANLIB=$TOOLCHAIN/bin/$TARGET-ranlib
export STRIP=$TOOLCHAIN/bin/$TARGET-strip
export PATH=$PATH:$TOOLCHAIN/bin
cd contrib
mkdir native
cd native
../bootstrap --build=x86_64-pc-linux-gnu --host=$TARGET$ANDROID_API
make
```

3) Update directories in the file /cross-files/android_arm64_api29.txt
4) Compile the library libjami.so

```bash
cd ../../
mkdir build
meson --cross-file `pwd`/cross-files/android_arm64_api29.txt build
cd build
ninja
ninja install
```

Note: to build the tests add `-Dtests=true` ; or it can be enabled later with `meson --reconfigure -Dtests=true build`

How to compile on macOS
----

These first steps are only necessary if you don't use a package manager.
```bash
cd extras/tools
./bootstrap
make
export PATH=$PATH:/location/of/daemon/extras/tools/build/bin
```

Or, use your favorite package manager to install the necessary tools
(macports or brew):
`automake pkg-config libtool gettext yasm`

How to compile on Windows
----

First, obtain and install `pywinmake` which is used to build the dependencies.

```bash
git clone "https://review.jami.net/pywinmake"
cd pywinmake
python -m pip install .
```

The rest of the build process uses CMake.

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

# Compile the dependencies
```bash
cd contrib
mkdir native
cd native
../bootstrap
make -j
```

# Then the daemon
```bash
cd ../../
./autogen.sh
./configure  --without-dbus --prefix=<install_path>
make
```

If you want to link against libjamiclient and native client easiest way is to
add to ./configure: --prefix=<prefix_path>

Do a little dance!

How to compile in a Docker container
----

docker build --tag jami-daemon .

# To build with custom build args

```bash
docker build --tag jami-daemon --build-arg cmake_args="-DJAMI_NODEJS=On" .
```

Common Issues
----

autopoint not found: When using Homebrew, autopoint is not found even when
gettext is installed, because symlinks are not created.
Run: 'brew link --force gettext' to fix it.


