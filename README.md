# Introduction

Jami is a voice-over-IP software phone.
Features include:
- user-friendly (fast, sleek, easy-to-learn interface)
- professional grade (file/call transfers, hold/resume, optimal audio quality, individual/group calls)
- compatible with Asterisk (using SIP account)
- distributed calls (P2P-DHT)
- customizable

As the SIP/audio daemon and the user interface are separate processes, different user interfaces can be easily applied to the daemon.
Jami comes with various graphical user interfaces and even scripts to control the daemon from the shell.

Jami is currently used by the support team of Savoir-faire Linux Inc.

More information is available on the project homepage:
 - https://www.jami.net/

This source tree contains only the daemon system process that handles the business logic of Jami.
User interfaces are located in different repositories.
See the [Contributing](#contributing) section for more information.

# Short description of the contents of the source tree

- `src/` is the core of libjami.
- `bin/` contains application and binding main code.
- `bin/dbus` contains the D-Bus XML interfaces, and C++ bindings

# About Savoir-faire Linux Inc.

Savoir-faire Linux Inc. is a consulting company based in Montreal, Quebec.
For more information, please visit the following website:
 - https://www.savoirfairelinux.com/

# How to compile on Linux

## A) With CMake

```bash
mkdir build
cd build
cmake .. -DJAMI_DBUS=On
make -j4
```

This should build the `contrib` dependencies, then the daemon.

## B) With Meson

1. Compile the dependencies.
   ```bash
   cd contrib
   mkdir build
   cd build
   ../bootstrap
   make
   ```

2. Compile the jamid system process and/or libjami library.
   ```bash
   cd ../../
   mkdir build
   export PATH=$PATH:`pwd`/contrib/`cc -dumpmachine`/bin
   meson setup -Dpkg_config_path=`pwd`/contrib/`cc -dumpmachine`/lib/pkgconfig -Ddefault_library=static -Dinterfaces=dbus build
   cd build
   ninja
   ninja install
   ```

# How to compile the daemon for Android (on a Linux or macOS host)

## A) With CMake

```bash
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=$ANDROID_NDK/build/cmake/android.toolchain.cmake -DANDROID_ABI=arm64-v8a -DANDROID_API=24 -DBUILD_EXTRA_TOOLS=On -DJAMI_JNI=On -DJAMI_JNI_PACKAGEDIR=java
make -j4
```

Replace arm64-v8a with the desired target ABI.
See the README in jami-client-android for instructions to build the Jami client for Android.

## B) With Meson

1. Download and install the Android NDK.
2. Compile the dependencies.
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

3. Update directories in the **/cross-files/android_arm64_api29.txt** file.
4. Compile the **libjami.so** library.
   ```bash
   cd ../../
   mkdir build
   meson --cross-file `pwd`/cross-files/android_arm64_api29.txt build
   cd build
   ninja
   ninja install
   ```

```{note}
To build the tests, add `-Dtests=true` or enable it later with `meson --reconfigure -Dtests=true build`.
```

# How to compile on macOS

These first steps are only necessary if a package manager is not used.

```bash
cd extras/tools
./bootstrap
make
export PATH=$PATH:/location/of/daemon/extras/tools/build/bin
```

Or, use your favorite package manager to install the necessary tools (MacPorts or Brew):
`automake pkg-config libtool gettext yasm`

# How to compile on Windows

First, obtain and install `pywinmake`, which is used to build the dependencies.

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
mkdir build
cd build
../bootstrap
make -j
```

# Compile the daemon

```bash
cd ../../
./autogen.sh
./configure  --without-dbus --prefix=<install_path>
make
```

If you want to link against libjamiclient and native client, the easiest way is to add to `./configure: --prefix=<prefix_path>`.

Do a little dance!

# How to compile in a Docker container

```bash
docker build --tag jami-daemon .
```

# To build with custom build arguments

```bash
docker build --tag jami-daemon --build-arg cmake_args="-DJAMI_NODEJS=On" .
```

# Common Issues

autopoint not found: When using Homebrew, autopoint is not found even when
gettext is installed, because symlinks are not created.
Run: `brew link --force gettext` to fix it.

# Contributing

Of course we love patches. And contributions. And spring rolls.

Development website and issue tracker:
 - https://git.jami.net/savoirfairelinux/

Repositories are hosted on Gerrit, which are used for code review.
It also contains the client subprojects:
 - https://review.jami.net/admin/repos/

Join us and post comments, suggestions, questions, and general feedback on the Jami mailing list:
 - https://lists.gnu.org/mailman/listinfo/jami

# COPYRIGHT NOTICE

Copyright (C) 2004-2026 Savoir-faire Linux Inc.

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
