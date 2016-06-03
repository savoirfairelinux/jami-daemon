#! /bin/sh

export BUILDFORIOS=1
MIN_IOS_VERSION=8.0
IOS_TARGET_PLATFORM=iPhoneSimulator

while test -n "$1"
do
  case "$1" in
  --platform=*)
  IOS_TARGET_PLATFORM="${1#--platform=}"
  ;;
  --host=*)
  HOST="${1#--host=}"
  ;;
  esac
  shift
done

if test -z "$HOST"
then
  if [ "$IOS_TARGET_PLATFORM" = "iPhoneSimulator" ]
  then
    ARCHS="x86_64"
  elif [ "$IOS_TARGET_PLATFORM" = "iPhoneOS" ]
  then
    ARCHS="armv7 arm64"
  fi
else
	ARCHS="${HOST%%-*}"
  case "$HOST" in
    armv7-*)
    IOS_TARGET_PLATFORM="iPhoneOS"
    ;;
		aarch64-*)
    IOS_TARGET_PLATFORM="iPhoneOS"
		ARCHS="arm64"
    ;;
    x86_64-*)
    IOS_TARGET_PLATFORM="iPhoneSimulator"
    ;;
  esac
fi

export IOS_TARGET_PLATFORM
echo "Building for $IOS_TARGET_PLATFORM for $ARCHS"

SDKROOT=`xcode-select -print-path`/Platforms/${IOS_TARGET_PLATFORM}.platform/Developer/SDKs/${IOS_TARGET_PLATFORM}${SDK_VERSION}.sdk

for ARCH in $ARCHS
do

	mkdir -p contrib/native-$ARCH
	cd contrib/native-$ARCH

	SDKROOT="$SDKROOT" ../bootstrap --host="$HOST" --disable-libav --disable-ffmepg

	echo "Building contrib"
	make -j4

	cd ../..
	echo "Building daemon"

	CC="xcrun clang -arch $ARCH -isysroot $SDKROOT -miphoneos-version-min=$MIN_IOS_VERSION"
	CXX="xcrun clang++ -std=c++11 -stdlib=libc++ -arch $ARCH -isysroot $SDKROOT -miphoneos-version-min=$MIN_IOS_VERSION"
	LDFLAGS="-arch $ARCH -isysroot $SDKROOT"
  EXTRA_CFLAGS+="-arch $ARCH -isysroot $SDKROOT -miphoneos-version-min=$MIN_IOS_VERSION"
	EXTRA_CXXFLAGS+="-std=c++11 -stdlib=libc++ -arch $ARCH -isysroot $SDKROOT -miphoneos-version-min=$MIN_IOS_VERSION"
	./autogen.sh
	mkdir -p "build-ios-$ARCH"
	./configure --host=$HOST \
							--disable-video \
							--without-dbus \
							CC="$CC" \
							CXX="$CXX" \
							EXTRA_CFLAGS="$EXTRA_CFLAGS" \
							EXTRA_CXXFLAGS="$EXTRA_CXXFLAGS" \
							LDFLAGS="$LDFLAGS" \
							--prefix=`pwd`/build-ios-$ARCH
	make
	make install
done

# lipo -create "build-ios-$ARCHS[0]/lib/libring.a"  \
# 						 "build-ios-$ARCHS[1]/lib/libring.a" \
# 						 -output "build-ios-$IOS_TARGET_PLATFORM/lib/libring.a"
