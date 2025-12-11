{
  description = "Jami Daemon - A distributed communication platform";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/nixos-unstable";
    flake-utils.url = "github:numtide/flake-utils";
  };

  outputs = { self, nixpkgs, flake-utils }:
    flake-utils.lib.eachDefaultSystem (system:
      let
        pkgs = import nixpkgs {
          inherit system;
          config.allowUnfree = true;
        };

        # Common build inputs for all build systems
        commonBuildInputs = with pkgs; [
          # Build tools
          pkg-config
          cmake
          meson
          ninja
          autoconf
          automake
          libtool
          gettext
          m4
          which
          perl
          nasm
          
          # Core dependencies
          gnutls
          libtasn1
          p11-kit
          nettle
          zlib
          libgit2
          pcre2
          secp256k1
          fmt
          yaml-cpp
          jsoncpp
          msgpack-cxx
          openssl
          libargon2
          
          # System integration
          systemd
          
          # FFmpeg and multimedia
          ffmpeg_6
          libopus
          libvpx
          x264
          
          # Audio libraries
          alsa-lib
          libpulseaudio
          pipewire
          libjack2
          portaudio
          speex
          speexdsp
          webrtc-audio-processing
          
          # Video support (Linux)
          libudev-zero
          libva
          libvdpau
          
          # Archive support
          libarchive
          minizip-ng
          
          # Networking and cryptography
          # Note: asio is header-only, needed for CMake to find it
          asio
          http-parser
          llhttp
          simdutf
          
          # Additional dependencies
          gmp
          liburcu
          cppunit
          
          # Documentation and version control
          git
        ];

        # D-Bus dependencies
        dbusDependencies = with pkgs; [
          dbus
          sdbus-cpp
        ];

        # Node.js dependencies for bindings
        nodejsDependencies = with pkgs; [
          nodejs
          nodePackages.node-gyp
          swig
        ];

        # Development shell dependencies
        devDependencies = with pkgs; [
          # Code quality tools
          clang-tools
          gdb
          valgrind
          
          # Build system tools
          ccache
          sccache
          
          # Utilities
          jq
          yq
          curl
          wget
          
          # Shell enhancements
          direnv
          nix-direnv
        ];

      in
      {
        # Development shell
        devShells.default = pkgs.mkShell {
          name = "jami-daemon-dev";
          
          buildInputs = commonBuildInputs 
            ++ dbusDependencies 
            ++ devDependencies;

          nativeBuildInputs = with pkgs; [
            pkg-config
            cmake
            meson
            ninja
            autoconf
            automake
            libtool
          ];

          shellHook = ''
            echo "🔔 Jami Daemon Development Environment"
            echo "======================================"
            echo ""
            echo "Available build systems:"
            echo "  - CMake:    mkdir build && cd build && cmake .. -DJAMI_DBUS=On && make -j$(nproc)"
            echo "  - Meson:    See instructions below"
            echo "  - Autotools: ./autogen.sh && ./configure && make"
            echo ""
            echo "For CMake:"
            echo "  mkdir -p build && cd build"
            echo "  cmake .. -DJAMI_DBUS=On -DBUILD_SHARED_LIBS=OFF"
            echo "  make -j$(nproc)"
            echo ""
            echo "For Meson (recommended):"
            echo "  1. Build contrib dependencies:"
            echo "     cd contrib && mkdir -p native && cd native"
            echo "     ../bootstrap && make -j$(nproc)"
            echo "     cd ../.."
            echo "     direnv reload  # Reload environment to pick up contrib"
            echo ""
            echo "  2. Build daemon:"
            echo "     mkdir -p build"
            echo "     export PATH=\$PATH:\$(pwd)/contrib/\$(cc -dumpmachine)/bin"
            echo "     meson setup build -Dpkg_config_path=\$(pwd)/contrib/\$(cc -dumpmachine)/lib/pkgconfig -Ddefault_library=static -Dinterfaces=dbus"
            echo "     cd build && ninja"
            echo ""
            echo "Environment variables:"
            echo "  CC=${pkgs.stdenv.cc}/bin/cc"
            echo "  CXX=${pkgs.stdenv.cc}/bin/c++"
            echo "  PKG_CONFIG_PATH=${pkgs.lib.makeSearchPath "lib/pkgconfig" (commonBuildInputs)}"
            echo ""
            echo "Jami version: ${self.packages.${system}.jami-daemon.version or "16.0.0"}"
            echo ""
            echo "⚠️  NOTE: These packages must be built from contrib/"
            echo "   - opendht (with Jami-specific version)"
            echo "   - dhtnet (custom Jami package)"
            echo "   - pjproject (custom fork with Jami patches)"
            echo "   - restinio (header-only with custom version)"
            echo "   - asio (header-only with custom version)"
            echo "   Run: nix run .#build-contrib"
            echo ""
            
            # Set up environment
            export PKG_CONFIG_PATH="${pkgs.lib.makeSearchPath "lib/pkgconfig" commonBuildInputs}"
            export CPATH="${pkgs.lib.makeSearchPath "include" commonBuildInputs}"
            export CMAKE_PREFIX_PATH="${pkgs.lib.concatStringsSep ":" commonBuildInputs}"
            
            # Force CMake to prefer static libraries by only exposing static library paths
            # CMake's find_library will search CMAKE_LIBRARY_PATH before system paths
            export CMAKE_LIBRARY_PATH="${pkgs.lib.makeLibraryPath commonBuildInputs}"
            
            # Force CMake to find static libraries first
            export CMAKE_FIND_LIBRARY_SUFFIXES=".a"
            
            # Add contrib to PKG_CONFIG_PATH if it exists
            CONTRIB_ARCH=$(${pkgs.stdenv.cc}/bin/cc -dumpmachine 2>/dev/null || echo "x86_64-unknown-linux-gnu")
            CONTRIB_DIR="$(pwd)/contrib/$CONTRIB_ARCH"
            if [ -d "$CONTRIB_DIR" ]; then
              export PKG_CONFIG_PATH="$CONTRIB_DIR/lib/pkgconfig:$PKG_CONFIG_PATH"
              export CMAKE_LIBRARY_PATH="$CONTRIB_DIR/lib:$CMAKE_LIBRARY_PATH"
              export CPATH="$CONTRIB_DIR/include:$CPATH"
              export PATH="$CONTRIB_DIR/bin:$PATH"
              export CMAKE_PREFIX_PATH="$CONTRIB_DIR:$CMAKE_PREFIX_PATH"
              echo "✓ Found contrib directory: $CONTRIB_DIR"
            fi
            
            # Enable ccache if available
            if command -v ccache > /dev/null; then
              export CC="ccache gcc"
              export CXX="ccache g++"
              export CCACHE_DIR="$HOME/.ccache/jami-daemon"
              mkdir -p "$CCACHE_DIR"
            fi
          '';
        };

        # Alternative shells for specific use cases
        devShells.nodejs = pkgs.mkShell {
          name = "jami-daemon-nodejs";
          buildInputs = commonBuildInputs 
            ++ nodejsDependencies 
            ++ devDependencies;
          
          shellHook = ''
            echo "🔔 Jami Daemon Development Environment (Node.js bindings)"
            export PKG_CONFIG_PATH="${pkgs.lib.makeSearchPath "lib/pkgconfig" commonBuildInputs}"
          '';
        };

        devShells.minimal = pkgs.mkShell {
          name = "jami-daemon-minimal";
          buildInputs = commonBuildInputs;
          
          shellHook = ''
            echo "🔔 Jami Daemon Minimal Development Environment"
            export PKG_CONFIG_PATH="${pkgs.lib.makeSearchPath "lib/pkgconfig" commonBuildInputs}"
          '';
        };

        # Package definition for the daemon itself
        packages.jami-daemon = pkgs.stdenv.mkDerivation {
          pname = "jami-daemon";
          version = "16.0.0";

          src = ./.;

          nativeBuildInputs = with pkgs; [
            pkg-config
            cmake
            autoconf
            automake
            libtool
            perl
          ];

          buildInputs = commonBuildInputs ++ dbusDependencies;

          cmakeFlags = [
            "-DJAMI_DBUS=On"
            "-DBUILD_SHARED_LIBS=OFF"
            "-DJAMI_VIDEO=On"
            "-DJAMI_PLUGIN=On"
          ];

          meta = with pkgs.lib; {
            description = "Jami daemon - Backend for the Jami communication platform";
            homepage = "https://jami.net/";
            license = licenses.gpl3Plus;
            platforms = platforms.linux;
            maintainers = [ ];
          };
        };

        packages.default = self.packages.${system}.jami-daemon;

        # Formatter for the flake
        formatter = pkgs.nixpkgs-fmt;

        # Apps that can be run with `nix run`
        apps.build-contrib = {
          type = "app";
          program = toString (pkgs.writeShellScript "build-contrib" ''
            set -e
            echo "Building Jami contrib dependencies..."
            echo "This will build dhtnet, pjproject, and other custom Jami packages."
            echo ""
            
            cd contrib
            mkdir -p native
            cd native
            
            echo "Running bootstrap..."
            ../bootstrap
            
            echo ""
            echo "Building with $(nproc) parallel jobs..."
            make -j$(nproc)
            
            echo ""
            echo "✅ Contrib dependencies built successfully!"
            echo ""
            echo "Contrib installed to: $(pwd)/../$(${pkgs.stdenv.cc}/bin/cc -dumpmachine)"
            echo ""
            echo "You can now build the daemon with:"
            echo "  mkdir -p build && cd build"
            echo "  cmake .. -DJAMI_DBUS=On"
            echo "  make -j$(nproc)"
          '');
        };

        apps.clean = {
          type = "app";
          program = toString (pkgs.writeShellScript "clean" ''
            rm -rf build contrib/native contrib/*/
            echo "✅ Cleaned build artifacts"
          '');
        };
      }
    );
}


