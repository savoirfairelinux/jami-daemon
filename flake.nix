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
            cmake
            meson
            nasm
            ninja
            perl
            pkg-config
          ];

          buildInputs = with pkgs; [
            alsa-lib
            asio
            ccache
            clang-tools
            cppunit
            curl
            dbus
            expat
            fmt
            gdb
            git
            gmp
            gnutls
            http-parser
            jq
            jsoncpp
            libarchive
            libargon2
            libgit2
            libjack2
            libopus
            libpulseaudio
            libtasn1
            libudev-zero
            liburcu
            libva
            libvdpau
            libvpx
            llhttp
            minizip-ng
            msgpack-cxx
            nettle

            openssl
            p11-kit
            pcre2
            pipewire
            portaudio
            sdbus-cpp
            secp256k1
            simdutf
            speex
            speexdsp
            systemd
            valgrind
            webrtc-audio-processing
            x264
            yaml-cpp
            zlib
          ];

          shellHook =
            let
              otel = pkgs.opentelemetry-cpp.override {
                  enableHttp = true;
                  cxxStandard = "20";
                };
              deps = with pkgs; [
                gnutls libtasn1 p11-kit nettle zlib libgit2 pcre2 secp256k1
                fmt yaml-cpp jsoncpp msgpack-cxx openssl libargon2 systemd
                dbus sdbus-cpp ffmpeg_6 libopus libvpx x264 alsa-lib
                libpulseaudio pipewire libjack2 speex speexdsp
                webrtc-audio-processing libudev-zero libva libvdpau libarchive
                minizip-ng asio http-parser llhttp simdutf gmp liburcu
                cppunit expat otel otel.dev
              ];
              contribArch = "${pkgs.stdenv.cc}/bin/cc";
            in
            ''
              export PKG_CONFIG_PATH="${lib.makeSearchPath "lib/pkgconfig" deps}''${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"
              export CPATH="${lib.makeSearchPath "include" deps}''${CPATH:+:$CPATH}"
              export CMAKE_PREFIX_PATH="${lib.concatStringsSep ":" deps}''${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
              export CMAKE_LIBRARY_PATH="${lib.makeLibraryPath deps}''${CMAKE_LIBRARY_PATH:+:$CMAKE_LIBRARY_PATH}"
              export LD_LIBRARY_PATH="${lib.makeLibraryPath deps}''${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

              CONTRIB_ARCH=$(${contribArch} -dumpmachine 2>/dev/null || echo "x86_64-unknown-linux-gnu")
              CONTRIB_DIR="$(pwd)/contrib/$CONTRIB_ARCH"
              if [ -d "$CONTRIB_DIR" ]; then
                export PKG_CONFIG_PATH="$CONTRIB_DIR/lib/pkgconfig:$PKG_CONFIG_PATH"
                export CMAKE_LIBRARY_PATH="$CONTRIB_DIR/lib:$CMAKE_LIBRARY_PATH"
                export CPATH="$CONTRIB_DIR/include:$CPATH"
                export PATH="$CONTRIB_DIR/bin:$PATH"
                export CMAKE_PREFIX_PATH="$CONTRIB_DIR:$CMAKE_PREFIX_PATH"
              fi
            '';
        };
      });
    };
}
