{
  description = "Jami Daemon - A distributed communication platform";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
  };

  outputs = { self, nixpkgs }:
    let
      lib = nixpkgs.lib;
      systems = [ "x86_64-linux" "aarch64-linux" ];
      forEachSystem = f: lib.genAttrs systems (system:
        let
          pkgs = import nixpkgs { inherit system; };
        in
        f { inherit pkgs lib system; }
      );
    in
    {
      devShells = forEachSystem ({ pkgs, lib, system }: {
        default = pkgs.mkShell {
          nativeBuildInputs = with pkgs; [
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
            gdb
            jq
          ];

          buildInputs = with pkgs; [
            # ── Crypto & TLS ──
            gnutls          # >= 3.6.7
            nettle          # >= 3.0.0
            gmp
            libtasn1
            p11-kit
            openssl
            libargon2
            secp256k1       # ECDSA signatures

            # ── Serialization / data formats ──
            fmt             # >= 5.3
            jsoncpp         # >= 1.6.5
            yaml-cpp        # >= 0.5.1
            msgpack-cxx     # >= 5.0.0
            simdutf

            # ── Compression & archives ──
            zlib
            libarchive      # >= 3.4.0 (plugins)
            minizip-ng

            # ── Networking ──
            asio            # header-only
            http-parser
            llhttp
            restinio
            libupnp
            libnatpmp
            libressl        # needed by opendht contrib build

            # ── Git ──
            libgit2         # >= 1.1.0

            # ── Audio codecs & backends ──
            libopus
            speex
            speexdsp
            alsa-lib        # >= 1.0
            libpulseaudio   # >= 0.9.15
            libjack2
            portaudio
            webrtc-audio-processing_0_3

            # ── Video codecs ──
            libvpx
            x264

            # ── D-Bus ──
            dbus
            sdbus-cpp       # >= 2.0.0
            systemd         # provides libudev

            # ── Hardware acceleration ──
            libva
            libvdpau
            libdrm

            # ── Video capture & display (needed by ffmpeg contrib) ──
            pipewire        # libpipewire for screen capture
            libx11
            libxcb
            xcb-proto
            libxext
            libxfixes
            v4l-utils       # v4l2 input device

            # ── Other system libs ──
            libudev-zero
            liburcu
            pcre2
            expat
            curl

            # ── Testing ──
            cppunit         # >= 1.12

            # ── Tracing (optional) ──
            lttng-ust

            # ── For nix develop -i: ensure basic tools work ──
            coreutils
            findutils
            gnugrep
            gnused
            bash
          ];

          # mkShell with pkg-config in nativeBuildInputs automatically sets
          # PKG_CONFIG_PATH for all buildInputs. We only need to:
          # 1. Set CMAKE_PREFIX_PATH so CMake find_package() works (e.g. yaml-cpp)
          # 2. Set up the contrib directory for internally-built packages
          shellHook =
            let
              contribArch = "${pkgs.stdenv.cc}/bin/cc";
            in
            ''
              # Contrib directory setup (for ffmpeg, pjproject, opendht, dhtnet)
              CONTRIB_ARCH=$(${contribArch} -dumpmachine 2>/dev/null || echo "x86_64-unknown-linux-gnu")
              CONTRIB_DIR="$(pwd)/contrib/$CONTRIB_ARCH"
              if [ -d "$CONTRIB_DIR" ]; then
                export PKG_CONFIG_PATH="$CONTRIB_DIR/lib/pkgconfig''${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
                export CMAKE_PREFIX_PATH="$CONTRIB_DIR''${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
                export PATH="$CONTRIB_DIR/bin''${PATH:+:$PATH}"
              fi
            '';
        };
      });
    };
}
