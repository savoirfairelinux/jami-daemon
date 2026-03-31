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
          darwinBuildInputs = with pkgs; [
            portaudio
          ] ++ (with pkgs.darwin.apple_sdk.frameworks; [
            CoreFoundation
            CoreAudio
            AudioToolbox
            CoreVideo
            VideoToolbox
            CoreMedia
            AVFoundation
            Security
            SystemConfiguration
            Accelerate
            AppKit
            IOKit
          ]);

          # Shell hook to configure contrib directory for native builds
          contribShellHook = ''
            CONTRIB_ARCH=$(${pkgs.stdenv.cc}/bin/cc -dumpmachine 2>/dev/null || echo "unknown")
            CONTRIB_DIR="$(pwd)/contrib/$CONTRIB_ARCH"
            if [ -d "$CONTRIB_DIR" ]; then
              export PKG_CONFIG_PATH="$CONTRIB_DIR/lib/pkgconfig''${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
              export CMAKE_PREFIX_PATH="$CONTRIB_DIR''${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
              export PATH="$CONTRIB_DIR/bin''${PATH:+:$PATH}"
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
          mkAndroidShell = { abi, target, api ? "29" }: pkgs.mkShell {
            nativeBuildInputs = commonNativeBuildInputs ++ [
              androidSdk
              pkgs.swig       # needed for JNI binding generation
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

              # Set up contrib paths (CMake uses contrib/<target>,
              # Meson bootstrap with --host=<target><api> uses contrib/<target><api>)
              for CONTRIB_DIR in "$(pwd)/contrib/${target}" "$(pwd)/contrib/${target}${api}"; do
                if [ -d "$CONTRIB_DIR" ]; then
                  export PKG_CONFIG_PATH="$CONTRIB_DIR/lib/pkgconfig''${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
                  export CMAKE_PREFIX_PATH="$CONTRIB_DIR''${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
                  export PATH="$CONTRIB_DIR/bin''${PATH:+:$PATH}"
                fi
              done

              echo "Android cross-compilation environment ready"
              echo "  NDK:     $ANDROID_NDK"
              echo "  ABI:     ${abi}"
              echo "  API:     ${api}"
              echo "  TARGET:  $TARGET"
              echo ""
              echo "To build with CMake:"
              echo "  mkdir -p build && cd build"
              echo "  cmake .. \\"
              echo "    -DCMAKE_TOOLCHAIN_FILE=\$ANDROID_NDK/build/cmake/android.toolchain.cmake \\"
              echo "    -DANDROID_ABI=${abi} \\"
              echo "    -DANDROID_PLATFORM=${api} \\"
              echo "    -DJAMI_JNI=On \\"
              echo "    -DJAMI_JNI_PACKAGEDIR=java \\"
              echo "    -DBUILD_TESTING=OFF"
              echo "  make -j\$(nproc)"
            '';
          };

          # Android cross-compilation is only supported on x86_64-linux and
          # macOS hosts (the NDK ships prebuilt toolchains for these).
          isAndroidHostSupported = system == "x86_64-linux" || isDarwin;

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
          };

          android-armv7a = mkAndroidShell {
            abi = "armeabi-v7a";
            target = "armv7a-linux-androideabi";
          };

          android-x86_64 = mkAndroidShell {
            abi = "x86_64";
            target = "x86_64-linux-android";
          };
        }
      );
    };
}
