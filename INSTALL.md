# Installing and Building jami-daemon

## Dependencies

The following libraries and tools are required to build jami-daemon.
All of them can be built automatically from the `contrib/` tree by configuring
with `BUILD_CONTRIB=ON` (the default).  Alternatively, any subset can be
provided by your system package manager; the contrib build skips packages it
finds via pkg-config.

**Build tools (all platforms)**

- C++20 compiler (GCC ≥ 12 or Clang ≥ 15)
- CMake ≥ 3.16 or Meson + Ninja
- pkg-config, make, git
- automake, autoconf, libtool
- Python 3, gawk, which
- yasm or nasm

**Core libraries (all platforms)**

- GnuTLS
- Nettle
- OpenSSL / LibreSSL
- jsoncpp
- libfmt
- yaml-cpp
- simdutf
- libsecp256k1
- libgit2
- speexdsp
- libupnp
- libnatpmp
- webrtc-audio-processing 0.3.x *(optional — echo cancellation)*
- restinio *(required only with `BUILD_TESTING=ON` for namedirectory tests)*
- CppUnit *(required only with `BUILD_TESTING=ON`)*

**Linux**

- libudev
- libpulse, libasound, or libjack *(at least one audio backend)*
- libv4l / v4l-utils *(video capture, when `JAMI_VIDEO=ON`)*
- libdrm
- libxcb, libx11, libxext, libxfixes
- lttng-ust *(optional)*

**macOS**

- Xcode command line tools
- automake, pkg-config, libtool, gettext, yasm (via Homebrew or MacPorts)

**Android**

- Android NDK r25 or later

**Windows**

- Visual Studio 2022 with C++ workload
- Python 3 with `pywinmake`

---

## How to compile on Linux

### A) With CMake

```bash
mkdir build
cd build
cmake .. -DJAMI_DBUS=On
make -j$(nproc)
```

This automatically builds the `contrib` dependencies and then the daemon.

### B) With Meson

1. Compile the contrib dependencies:
   ```bash
   cd contrib
   mkdir build
   cd build
   ../bootstrap
   make
   ```

2. Compile the daemon:
   ```bash
   cd ../../
   mkdir build
   export PATH=$PATH:$(pwd)/contrib/$(cc -dumpmachine)/bin
   meson setup \
     -Dpkg_config_path=$(pwd)/contrib/$(cc -dumpmachine)/lib/pkgconfig \
     -Ddefault_library=static \
     -Dinterfaces=dbus \
     build
   cd build
   ninja
   ninja install
   ```

---

## How to compile the daemon for Android (on a Linux or macOS host)

### A) With CMake

```bash
mkdir build
cd build
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake \
  -DANDROID_ABI=arm64-v8a \
  -DANDROID_API=24 \
  -DBUILD_EXTRA_TOOLS=On \
  -DJAMI_JNI=On \
  -DJAMI_JNI_PACKAGEDIR=java
make -j$(nproc)
```

Replace `arm64-v8a` with the desired target ABI.
See the README in `jami-client-android` for full client-side build instructions.

### B) With Meson

1. Download and install the Android NDK.
2. Compile the contrib dependencies:
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
   mkdir build
   cd build
   ../bootstrap --build=x86_64-pc-linux-gnu --host=$TARGET$ANDROID_API
   make
   ```

3. Update the paths in `cross-files/android_arm64_api29.txt`.
4. Compile the library:
   ```bash
   cd ../../
   mkdir build
   meson --cross-file $(pwd)/cross-files/android_arm64_api29.txt build
   cd build
   ninja
   ninja install
   ```

> **Tests:** add `-Dtests=true` to the `meson setup` invocation, or enable later
> with `meson --reconfigure -Dtests=true build`.

---

## How to compile on macOS

If not using a package manager, first bootstrap the extra build tools:

```bash
cd extras/tools
./bootstrap
make
export PATH=$PATH:/location/of/daemon/extras/tools/build/bin
```

Or install the tools via Homebrew:

```bash
brew install automake pkg-config libtool gettext yasm
```

Then follow the standard Linux/CMake or Linux/Meson steps above.

> **Common issue:** If `autopoint` is not found even after installing `gettext`
> via Homebrew, run: `brew link --force gettext`

---

## How to compile on Windows

First obtain and install `pywinmake`, which builds the contrib dependencies:

```bash
git clone "https://review.jami.net/pywinmake"
cd pywinmake
python -m pip install .
```

Then build with CMake:

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

---

## How to compile in a Docker container

```bash
docker build --tag jami-daemon .
```

To pass custom CMake arguments:

```bash
docker build --tag jami-daemon --build-arg cmake_args="-DJAMI_NODEJS=On" .
```

---

## Building only the contrib dependencies

```bash
cd contrib
mkdir build
cd build
../bootstrap
make -j$(nproc)
```

---

## Notable build options

| Description | CMake | Meson |
|-------------|-------|-------|
| Build with video support | `JAMI_VIDEO=ON` | `-Dvideo=true` |
| Build the D-Bus daemon binding | `JAMI_DBUS=ON` | `-Dinterfaces=dbus` |
| Build with plugin support | `JAMI_PLUGIN=ON` | `-Dplugins=true` |
| Build a shared library | `BUILD_SHARED_LIBS=ON` | `-Ddefault_library=shared` |
| Build the unit-test suite | `BUILD_TESTING=ON` | `-Dtests=true` |
| Build contrib dependencies | `BUILD_CONTRIB=ON` | *(always manual, see above)* |
| Enable AddressSanitizer | `ENABLE_ASAN=ON` | `-Db_sanitize=address` |
| Enable coverage instrumentation | `ENABLE_COVERAGE=ON` | `-Db_coverage=true` |
| Enable tracepoints | — | `-Dtracepoints=true` |
| Hardware video acceleration | `JAMI_VIDEO_ACCEL=ON` | `-Dhw_acceleration=true` |
