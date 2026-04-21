#!/bin/sh
# ---------------------------------------------------------------------------
# compile-ios.sh — Build jami-daemon for iOS (iPhoneOS / iPhoneSimulator)
#
# This script is self-contained within the daemon repository.  It:
#   1. Bootstraps and builds contrib dependencies for the target arch/platform
#   2. Builds the daemon with CMake cross-compilation for iOS
#   3. Installs into DEPS/<arch>-<platform>/
#
# Usage:
#   ./compile-ios.sh --platform=iPhoneOS              # arm64 device
#   ./compile-ios.sh --platform=iPhoneSimulator        # arm64 + x86_64 sim
#   ./compile-ios.sh --platform=iPhoneSimulator --arch=arm64
#   ./compile-ios.sh --platform=all --release
#
# Environment:
#   NPROC           — parallelism (default: hw.ncpu)
#   TARBALLS        — path to pre-fetched source tarballs (optional)
#   RELEASE         — set to 1 for Release build (or use --release)
# ---------------------------------------------------------------------------
set -e

export BUILDFORIOS=1
export MIN_IOS_VERSION=14.5
IOS_TARGET_PLATFORM=iPhoneSimulator
RELEASE=${RELEASE:-0}

show_help() {
  echo "Usage: $0 [options]"
  echo ""
  echo "Build jami-daemon for iOS platforms"
  echo ""
  echo "Options:"
  echo "  --platform=PLATFORM   iPhoneOS | iPhoneSimulator | all"
  echo "  --arch=ARCH           arm64 | x86_64 (simulator only)"
  echo "  --release             Release build with optimizations"
  echo "  --help                Show this help"
  exit 0
}

while test -n "$1"; do
  case "$1" in
    --platform=*) IOS_TARGET_PLATFORM="${1#--platform=}" ;;
    --arch=*)
      case "${1#--arch=}" in
        arm64)  ARCH="aarch64-apple-darwin_ios" ;;
        x86_64) ARCH="x86_64-apple-darwin_ios" ;;
        *)      ARCH="${1#--arch=}" ;;
      esac
      ;;
    --release)  RELEASE=1 ;;
    --help)     show_help ;;
    *)          echo "Unknown option: $1"; exit 1 ;;
  esac
  shift
done

if [ "$IOS_TARGET_PLATFORM" = "all" ]; then
  ARCHS_PLATFORMS="arm64:iPhoneOS arm64:iPhoneSimulator x86_64:iPhoneSimulator"
elif [ "$IOS_TARGET_PLATFORM" = "iPhoneSimulator" ]; then
  if [ -n "$ARCH" ]; then
    case "$ARCH" in
      aarch64-*) ARCHS_PLATFORMS="arm64:iPhoneSimulator" ;;
      x86_64-*)  ARCHS_PLATFORMS="x86_64:iPhoneSimulator" ;;
      *)         ARCHS_PLATFORMS="arm64:iPhoneSimulator x86_64:iPhoneSimulator" ;;
    esac
  else
    ARCHS_PLATFORMS="arm64:iPhoneSimulator x86_64:iPhoneSimulator"
  fi
elif [ "$IOS_TARGET_PLATFORM" = "iPhoneOS" ]; then
  ARCHS_PLATFORMS="arm64:iPhoneOS"
fi

DAEMON_DIR="$(cd "$(dirname "$0")/../.." && pwd)"

# Ensure gas-preprocessor.pl is available (required for some FFmpeg asm)
if ! command -v gas-preprocessor.pl >/dev/null 2>&1; then
  echo 'gas-preprocessor.pl not found. Installing…'
  mkdir -p "$DAEMON_DIR/extras/tools/build/bin/"
  curl -L https://github.com/libav/gas-preprocessor/raw/master/gas-preprocessor.pl \
    -o "$DAEMON_DIR/extras/tools/build/bin/gas-preprocessor.pl"
  chmod +x "$DAEMON_DIR/extras/tools/build/bin/gas-preprocessor.pl"
  export PATH="$DAEMON_DIR/extras/tools/build/bin:$PATH"
fi

NPROC=${NPROC:-$(sysctl -n hw.ncpu 2>/dev/null || echo 4)}
OUTPUT_DIR="$DAEMON_DIR/build-ios-output"
DEPS_DIR="$OUTPUT_DIR/DEPS"

echo "Building for: $ARCHS_PLATFORMS"
echo "Parallelism:  $NPROC"

cd "$DAEMON_DIR"

for ARCH_PLATFORM in $ARCHS_PLATFORMS; do
  ARCH="${ARCH_PLATFORM%%:*}"
  PLATFORM="${ARCH_PLATFORM#*:}"
  export IOS_TARGET_PLATFORM="$PLATFORM"

  echo ""
  echo "======================================================================"
  echo "  Building: $ARCH / $PLATFORM"
  echo "======================================================================"

  BUILD_DIR="$ARCH-$PLATFORM"

  # --- Determine host triplet ---
  if [ "$ARCH" = "arm64" ]; then
    HOST=aarch64-apple-darwin_ios
  else
    HOST="${ARCH}-apple-darwin_ios"
  fi

  # --- SDK paths ---
  SDKROOT=$(xcode-select -print-path)/Platforms/${PLATFORM}.platform/Developer/SDKs/${PLATFORM}${SDK_VERSION}.sdk

  if [ "12.0" \> "$(sw_vers -productVersion)" ]; then
    SDK="$(echo "print '${PLATFORM}'.lower()" | python)"
  else
    SDK="$(echo "print('${PLATFORM}'.lower())" | python3)"
  fi

  CC="xcrun -sdk $SDK clang"
  CXX="xcrun -sdk $SDK clang++"
  CONTRIB_FOLDER="$DAEMON_DIR/contrib/$BUILD_DIR"

  if [ "$PLATFORM" = "iPhoneSimulator" ]; then
    MIN_IOS="-mios-simulator-version-min=$MIN_IOS_VERSION"
  else
    MIN_IOS="-miphoneos-version-min=$MIN_IOS_VERSION"
  fi

  DEVPATH=$(xcrun --sdk "$SDK" --show-sdk-platform-path)/Developer
  export DEVPATH MIN_IOS

  # --- Build contribs ---
  if [ ! -d "$CONTRIB_FOLDER/lib" ]; then
    echo "Building contribs for $BUILD_DIR…"
    mkdir -p "contrib/native-$BUILD_DIR"
    cd "contrib/native-$BUILD_DIR"
    SDKROOT="$SDKROOT" ../bootstrap \
      --host="$HOST" \
      --disable-libav --disable-plugin --disable-libarchive \
      --enable-ffmpeg \
      --prefix="$CONTRIB_FOLDER"
    make fetch
    make -j"$NPROC"
    cd "$DAEMON_DIR"
  else
    echo "Contribs already present at $CONTRIB_FOLDER, skipping."
  fi

  # --- Build daemon ---
  echo "Building daemon for $BUILD_DIR…"
  CMAKE_BUILD_DIR="$DAEMON_DIR/build-ios-$BUILD_DIR"
  INSTALL_PREFIX="$DEPS_DIR/$BUILD_DIR"

  if [ "$PLATFORM" = "iPhoneOS" ]; then
    DEPLOY_FLAG="-miphoneos-version-min=$MIN_IOS_VERSION"
    BITCODE_FLAG="-fembed-bitcode"
  else
    DEPLOY_FLAG="-mios-simulator-version-min=$MIN_IOS_VERSION"
    BITCODE_FLAG=""
  fi

  BUILD_TYPE="Debug"
  [ "$RELEASE" = "1" ] && BUILD_TYPE="Release"

  export PKG_CONFIG_PATH="$CONTRIB_FOLDER/lib/pkgconfig${PKG_CONFIG_PATH:+:$PKG_CONFIG_PATH}"

  cmake -S "$DAEMON_DIR" -B "$CMAKE_BUILD_DIR" \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT="$SDKROOT" \
    -DCMAKE_OSX_ARCHITECTURES="$ARCH" \
    -DCMAKE_OSX_DEPLOYMENT_TARGET="$MIN_IOS_VERSION" \
    -DCMAKE_INSTALL_PREFIX="$INSTALL_PREFIX" \
    -DCMAKE_C_FLAGS="$DEPLOY_FLAG $BITCODE_FLAG" \
    -DCMAKE_CXX_FLAGS="$DEPLOY_FLAG $BITCODE_FLAG" \
    -DCMAKE_FIND_ROOT_PATH="$CONTRIB_FOLDER" \
    -DCMAKE_PREFIX_PATH="$CONTRIB_FOLDER" \
    -DBUILD_SHARED_LIBS=OFF \
    -DBUILD_TESTING=OFF \
    -DJAMI_DBUS=OFF \
    -DJAMI_PLUGIN=OFF \
    -DBUILD_CONTRIB=OFF \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

  cmake --build "$CMAKE_BUILD_DIR" --parallel "$NPROC"
  cmake --install "$CMAKE_BUILD_DIR"

  # --- Copy contrib artifacts into output ---
  rsync -ar "$CONTRIB_FOLDER/lib/"*.a "$INSTALL_PREFIX/lib/"
  rsync -ar "$CONTRIB_FOLDER/include/opendht"    "$INSTALL_PREFIX/include/" 2>/dev/null || true
  rsync -ar "$CONTRIB_FOLDER/include/msgpack.hpp" "$INSTALL_PREFIX/include/" 2>/dev/null || true
  rsync -ar "$CONTRIB_FOLDER/include/gnutls"      "$INSTALL_PREFIX/include/" 2>/dev/null || true
  rsync -ar "$CONTRIB_FOLDER/include/json"        "$INSTALL_PREFIX/include/" 2>/dev/null || true
  rsync -ar "$CONTRIB_FOLDER/include/msgpack"     "$INSTALL_PREFIX/include/" 2>/dev/null || true
  rsync -ar "$CONTRIB_FOLDER/include/yaml-cpp"    "$INSTALL_PREFIX/include/" 2>/dev/null || true
  rsync -ar "$CONTRIB_FOLDER/include/libavutil"   "$INSTALL_PREFIX/include/" 2>/dev/null || true
  rsync -ar "$CONTRIB_FOLDER/include/fmt"         "$INSTALL_PREFIX/include/" 2>/dev/null || true

  # Strip host suffix from archive names
  cd "$INSTALL_PREFIX/lib/"
  for i in *.a; do mv "$i" "${i/-$HOST.a/.a}" 2>/dev/null || true; done
  cd "$DAEMON_DIR"
done

echo ""
echo "======================================================================"
echo "  Build complete.  Output in: $OUTPUT_DIR/DEPS/"
echo "======================================================================"
ls -la "$DEPS_DIR/" 2>/dev/null || true
