{
  description = "Jami Daemon - A distributed communication platform";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      lib = nixpkgs.lib;
      systems = [ "x86_64-linux" "aarch64-linux" "x86_64-darwin" "aarch64-darwin" ];
      forEachSystem = f: lib.genAttrs systems (system:
        let
          pkgs = import nixpkgs {
            inherit system;
            config = {
              allowUnfree = true;
              android_sdk.accept_license = true;
            };
          };
        in
        f { inherit pkgs lib system; }
      );
    in
    {
      devShells = forEachSystem ({ pkgs, lib, system }:
        let
          isLinux = pkgs.stdenv.isLinux;
          isDarwin = pkgs.stdenv.isDarwin;

          # Common native build tools (all platforms)
          commonNativeBuildInputs = with pkgs; [
            # Build systems
            cmake
            meson
            ninja
            pkg-config
            # Needed by contrib builds
            automake
            autoconf
            libtool
            nasm
            perl
            python3
            gnumake
            git
            gawk
            which
            # Tools
            ccache
            jq
            wget
          ];

          # Common library dependencies (all platforms)
          commonBuildInputs = with pkgs; [
            # Crypto & TLS
            gnutls          # >= 3.6.7
            nettle          # >= 3.0.0
            gmp
            libtasn1
            p11-kit
            openssl
            libargon2
            secp256k1       # ECDSA signatures

            # Serialization / data formats
            fmt             # >= 5.3
            jsoncpp         # >= 1.6.5
            yaml-cpp        # >= 0.5.1
            msgpack-cxx     # >= 5.0.0
            simdutf

            # Compression & archives
            zlib
            libarchive      # >= 3.4.0 (plugins)
            minizip-ng

            # Networking
            asio            # header-only
            http-parser
            llhttp
            restinio
            libupnp
            libnatpmp
            libressl        # needed by opendht contrib build

            # Git
            libgit2         # >= 1.1.0

            # Audio codecs
            libopus
            speex
            speexdsp

            # Video codecs
            libvpx
            x264

            # Other
            pcre2
            expat
            curl

            # Telemetry
            protobuf
            (opentelemetry-cpp.override {
              enableHttp = true;
              cxxStandard = "20";
            })

            # Testing
            cppunit         # >= 1.12
          ];

          # Linux-only libraries
          linuxBuildInputs = with pkgs; [
            # Audio backends
            alsa-lib        # >= 1.0
            libpulseaudio   # >= 0.9.15
            libjack2
            portaudio
            webrtc-audio-processing_0_3

            # D-Bus
            dbus
            sdbus-cpp       # >= 2.0.0
            systemd         # provides libudev

            # Hardware acceleration
            libva
            libvdpau
            libdrm

            # Video capture & display
            pipewire        # libpipewire for screen capture
            libx11
            libxcb
            xcb-proto
            libxext
            libxfixes
            v4l-utils       # v4l2 input device

            # Other system libs
            libudev-zero
            liburcu

            # Tracing (optional)
            lttng-ust
          ];

          # macOS-only libraries
          # Frameworks are provided automatically by the default SDK in stdenv.
          # If a non-default SDK is needed, add e.g. `apple-sdk_15` to buildInputs.
          darwinBuildInputs = with pkgs; [
            portaudio
          ];

          # Shell hook to configure contrib directory for native builds
          contribShellHook = ''
            CONTRIB_ARCH=$(${pkgs.stdenv.cc}/bin/cc -dumpmachine 2>/dev/null || echo "unknown")
            CONTRIB_DIR="$(pwd)/contrib/$CONTRIB_ARCH"
            export CONTRIB_PREFIX="$CONTRIB_DIR"
            if [ -d "$CONTRIB_DIR" ]; then
              export PKG_CONFIG_PATH="$CONTRIB_DIR/lib/pkgconfig''${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
              export CMAKE_PREFIX_PATH="$CONTRIB_DIR''${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
              export PATH="$CONTRIB_DIR/bin''${PATH:+:$PATH}"
              echo "Contrib prefix: $CONTRIB_DIR"
              echo "PKG_CONFIG_PATH and CMAKE_PREFIX_PATH configured."
            else
              echo "NOTE: contrib dir '$CONTRIB_DIR' not found."
              echo "      Build contribs first: cd contrib && make"
            fi
          '';

          # Android NDK setup (lazy - only evaluated when Android shells are used)
          ndkVersion = "26.3.11579264";
          androidComposition = pkgs.androidenv.composeAndroidPackages {
            includeNDK = true;
            ndkVersions = [ ndkVersion ];
          };
          androidSdk = androidComposition.androidsdk;
          androidNdk = "${androidSdk}/libexec/android-sdk/ndk/${ndkVersion}";
          ndkHostTag = if isLinux then "linux-x86_64" else "darwin-x86_64";

          # Android cross-compilation shell factory.
          # Mirrors the environment described in INSTALL.md § "How to compile
          # the daemon for Android".  All target libraries come from contrib;
          # only host build tools are supplied by Nix.
          mkAndroidShell = { abi, target, api ? "29", mesonCpuFamily, mesonCpu }: pkgs.mkShell {
            nativeBuildInputs = commonNativeBuildInputs ++ [
              androidSdk
              pkgs.swig       # needed for JNI binding generation
              # Host protoc must match the cross-compiled libprotobuf 3.21.12
              # in contribs. Using protobuf_21 (3.21.x series) ensures the
              # .pb.cc files generated by protoc are ABI-compatible with it.
              pkgs.protobuf_21
            ];

            ANDROID_NDK = androidNdk;
            ANDROID_ABI = abi;
            ANDROID_API = api;

            shellHook = ''
              export TOOLCHAIN="$ANDROID_NDK/toolchains/llvm/prebuilt/${ndkHostTag}"
              export TARGET="${target}"
              export CC="$TOOLCHAIN/bin/${target}${api}-clang"
              export CXX="$TOOLCHAIN/bin/${target}${api}-clang++"
              export AR="$TOOLCHAIN/bin/llvm-ar"
              export LD="$TOOLCHAIN/bin/ld"
              export RANLIB="$TOOLCHAIN/bin/llvm-ranlib"
              export STRIP="$TOOLCHAIN/bin/llvm-strip"
              export NM="$TOOLCHAIN/bin/llvm-nm"
              export PATH="$TOOLCHAIN/bin''${PATH:+:$PATH}"
              export BUILD_TESTING="OFF"

              # Set up contrib paths
              CONTRIB_DIR="$(pwd)/contrib/${target}"
              if [ -d "$CONTRIB_DIR" ]; then
                export PKG_CONFIG_PATH="$CONTRIB_DIR/lib/pkgconfig''${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
                export CMAKE_PREFIX_PATH="$CONTRIB_DIR''${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
                export PATH="$CONTRIB_DIR/bin''${PATH:+:$PATH}"
              fi

              # Generate a meson cross-file from the env vars set above.
              # This avoids static files with <daemon> placeholders and ensures
              # tool paths always match what the shell actually exports.
              _MESON_CONTRIB_DIR="$(pwd)/contrib/${target}"
              MESON_CROSS_FILE="$(pwd)/cross-files/android_${abi}_api${api}.ini"
              mkdir -p "$(pwd)/cross-files"
              {
                echo "# Auto-generated by nix devShell — do not edit"
                echo '[host_machine]'
                echo "system = 'linux'"
                echo "cpu_family = '${mesonCpuFamily}'"
                echo "cpu = '${mesonCpu}'"
                echo "endian = 'little'"
                echo ""
                echo '[binaries]'
                echo "c = '$CC'"
                echo "cpp = '$CXX'"
                echo "ar = '$AR'"
                echo "strip = '$STRIP'"
                echo "pkg-config = 'pkg-config'"
                echo "cmake = '$(which cmake)'"
                echo ""
                echo '[properties]'
                echo "pkg_config_libdir = '$_MESON_CONTRIB_DIR/lib/pkgconfig'"
                echo "cmake_prefix_path = ['$_MESON_CONTRIB_DIR']"
              } > "$MESON_CROSS_FILE"
              export MESON_CROSS_FILE

              echo "Android cross-compilation environment ready"
              echo "  NDK:     $ANDROID_NDK"
              echo "  ABI:     ${abi}"
              echo "  API:     ${api}"
              echo "  TARGET:  $TARGET"
              echo ""
              echo "To build with CMake:"
              echo "  cmake -S . -B build \\"
              echo "    -DCMAKE_TOOLCHAIN_FILE=\$ANDROID_NDK/build/cmake/android.toolchain.cmake \\"
              echo "    -DANDROID_ABI=${abi} \\"
              echo "    -DANDROID_PLATFORM=${api} \\"
              echo "    -DJAMI_JNI=On -DJAMI_JNI_PACKAGEDIR=java -DBUILD_TESTING=OFF"
              echo "  make -j\$(nproc)"
              echo ""
              echo "To build with Meson (cross-file: \$MESON_CROSS_FILE):"
              echo "  meson setup --cross-file \$MESON_CROSS_FILE \\"
              echo "    -Ddefault_library=static -Dinterfaces=jni build"
              echo "  ninja -C build"
            '';
          };

          # Android cross-compilation is only supported on x86_64-linux and
          # macOS hosts (the NDK ships prebuilt toolchains for these).
          isAndroidHostSupported = system == "x86_64-linux" || isDarwin;

          # iOS cross-compilation shell factory.
          # Nix supplies the build-host tools (cmake, autoconf, nasm, etc.).
          # Xcode supplies the actual cross-compiler and iOS/simulator SDKs.
          mkIOSShell = { platform, arch ? "arm64", minVersion ? "14.5" }:
            let
              hostTriple = if arch == "arm64"
                then "aarch64-apple-darwin_ios"
                else "${arch}-apple-darwin_ios";
              buildDir = "${arch}-${platform}";
              isSimulator = platform == "iPhoneSimulator";
              sdkFlag = if isSimulator
                then "-mios-simulator-version-min=${minVersion}"
                else "-miphoneos-version-min=${minVersion}";
            in pkgs.mkShell {
              nativeBuildInputs = commonNativeBuildInputs;

              shellHook = ''
                export BUILDFORIOS=1
                export IOS_TARGET_PLATFORM="${platform}"
                export MIN_IOS_VERSION="${minVersion}"
                export HOST="${hostTriple}"
                export BUILD_DIR="${buildDir}"

                # Xcode SDK path — unset DEVELOPER_DIR first because Nix's
                # Darwin stdenv sets it to the Nix store SDK which has no iOS
                # platforms. We need the real Xcode installation.
                unset DEVELOPER_DIR
                XCODE_DEV="$(/usr/bin/xcode-select -print-path 2>/dev/null || echo /Applications/Xcode.app/Contents/Developer)"
                export SDKROOT="$XCODE_DEV/Platforms/${platform}.platform/Developer/SDKs/${platform}.sdk"

                # pjproject's configure-iphone defaults DEVPATH to iPhoneOS
                # and MIN_IOS to -miphoneos-version-min, which is wrong for
                # simulator builds. Export correct values for the platform.
                export DEVPATH="$XCODE_DEV/Platforms/${platform}.platform/Developer"
                export MIN_IOS="${sdkFlag}"

                # x264 needs clang as AS (not /usr/bin/as) so .S files get
                # preprocessed before assembly.
                export AS="xcrun clang"

                if [ ! -d "$SDKROOT" ]; then
                  echo "WARNING: SDKROOT=$SDKROOT does not exist."
                  echo "         Ensure Xcode with iOS SDKs is installed."
                fi

                # Build-host compiler — autotools packages like GMP need to
                # compile+run programs on the macOS host during the build.
                # Use Nix's own clang which has its sysroot baked in and
                # is immune to the SDKROOT env var.
                export CC_FOR_BUILD="${pkgs.stdenv.cc}/bin/cc"
                export CXX_FOR_BUILD="${pkgs.stdenv.cc}/bin/c++"
                export BUILD_CC="${pkgs.stdenv.cc}/bin/cc"

                # Ensure Xcode tools (xcrun, clang, ld, etc.) are reachable
                export PATH="/usr/bin:$XCODE_DEV/usr/bin''${PATH:+:$PATH}"

                # Set up contrib paths
                CONTRIB_DIR="$(pwd)/contrib/${buildDir}"
                export CONTRIB_PREFIX="$CONTRIB_DIR"
                if [ -d "$CONTRIB_DIR" ]; then
                  export PKG_CONFIG_PATH="$CONTRIB_DIR/lib/pkgconfig''${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
                  export CMAKE_PREFIX_PATH="$CONTRIB_DIR''${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
                  export PATH="$CONTRIB_DIR/bin''${PATH:+:$PATH}"
                fi

                echo "iOS cross-compilation environment ready"
                echo "  Platform:  ${platform}"
                echo "  Arch:      ${arch}"
                echo "  Host:      ${hostTriple}"
                echo "  SDKROOT:   $SDKROOT"
                echo "  Min iOS:   ${minVersion}"
                echo ""
                echo "To build contribs:"
                echo "  mkdir -p contrib/native-${buildDir} && cd contrib/native-${buildDir}"
                echo "  ../bootstrap --host=$HOST \\"
                echo "    --disable-libav --disable-plugin --disable-libarchive --enable-ffmpeg \\"
                echo "    --prefix=\$(pwd)/../${buildDir}"
                echo "  make fetch && make -j\$(sysctl -n hw.ncpu)"
                echo ""
                echo "To build daemon with CMake:"
                echo "  cmake -S . -B build-ios-${buildDir} \\"
                echo "    -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=\$SDKROOT \\"
                echo "    -DCMAKE_OSX_ARCHITECTURES=${arch} -DCMAKE_OSX_DEPLOYMENT_TARGET=${minVersion} \\"
                echo "    -DBUILD_SHARED_LIBS=OFF -DBUILD_TESTING=OFF -DJAMI_DBUS=OFF -DJAMI_PLUGIN=OFF \\"
                echo "    -DBUILD_CONTRIB=OFF"
                echo "  cmake --build build-ios-${buildDir} -j\$(sysctl -n hw.ncpu)"
              '';
            };

        in
        {
          # Default native development shell
          default = pkgs.mkShell {
            nativeBuildInputs = commonNativeBuildInputs
              ++ lib.optionals isLinux [ pkgs.gdb ];

            buildInputs = commonBuildInputs
              ++ lib.optionals isLinux linuxBuildInputs
              ++ lib.optionals isDarwin darwinBuildInputs
              ++ [
                pkgs.coreutils
                pkgs.findutils
                pkgs.gnugrep
                pkgs.gnused
                pkgs.bash
              ];

            shellHook = contribShellHook;
          };
        }
        # Android cross-compilation shells
        // lib.optionalAttrs isAndroidHostSupported {
          android-arm64 = mkAndroidShell {
            abi = "arm64-v8a";
            target = "aarch64-linux-android";
            mesonCpuFamily = "aarch64";
            mesonCpu = "aarch64";
          };

          android-armv7a = mkAndroidShell {
            abi = "armeabi-v7a";
            target = "armv7a-linux-androideabi";
            mesonCpuFamily = "arm";
            mesonCpu = "armv7a";
          };

          android-x86_64 = mkAndroidShell {
            abi = "x86_64";
            target = "x86_64-linux-android";
            mesonCpuFamily = "x86_64";
            mesonCpu = "x86_64";
          };
        }
        # iOS cross-compilation shells (macOS hosts only — requires Xcode)
        // lib.optionalAttrs isDarwin {
          ios-device = mkIOSShell {
            platform = "iPhoneOS";
            arch = "arm64";
          };

          ios-simulator = mkIOSShell {
            platform = "iPhoneSimulator";
            arch = "arm64";
          };

          ios-simulator-x86_64 = mkIOSShell {
            platform = "iPhoneSimulator";
            arch = "x86_64";
          };
        }
      );
    };
}
