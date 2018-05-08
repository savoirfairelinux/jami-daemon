cmake_minimum_required(VERSION 3.11.0)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(PROJECT_DIR "${CMAKE_SOURCE_DIR}/../../..")
set(DAEMON_DIR "${PROJECT_DIR}/daemon")
set(CONTRIB_DIR "${DAEMON_DIR}/contrib")
set(TARGET_TUPLE "${ANDROID_ABI}")
set(JNI_DIR "${DAEMON_DIR}/bin/jni")

set(ANDROID_STL c++_static)

if (${ANDROID_ABI} MATCHES "arm64-v8a")
    set(TARGET_TUPLE_DRING "aarch64-linux-android")
    set(TARGET_TUPLE_PJ "aarch64-unknown-linux-android")
endif()
if (${ANDROID_ABI} MATCHES "armeabi-v7a")
    set(TARGET_TUPLE_DRING "arm-linux-androideabi")
    set(TARGET_TUPLE_PJ "arm-unknown-linux-androideabi")
endif()
if (${ANDROID_ABI} MATCHES "x86")
    set(TARGET_TUPLE_DRING "x86-linux-android")
    set(TARGET_TUPLE_PJ "i686-pc-linux-android")
endif()
if (${ANDROID_ABI} MATCHES "x86_64")
    set(TARGET_TUPLE_DRING "x86_64-linux-android")
    set(TARGET_TUPLE_PJ "x86_64-pc-linux-android")
endif()

set(DAEMON_BUILD_DIR "${DAEMON_DIR}/build-android-${TARGET_TUPLE_DRING}")
set(DAEMON_LIB "${DAEMON_BUILD_DIR}/src/.libs/libring.a")

add_custom_command(
    OUTPUT ${JNI_DIR}/ring_wrapper.cpp
    WORKING_DIRECTORY ${JNI_DIR}
    COMMAND PACKAGEDIR=${ANDROID_JNI_DIR} ${JNI_DIR}/make-swig.sh
)

add_custom_target(
    dring
    WORKING_DIRECTORY ${DAEMON_DIR}/bin/android
    COMMAND ANDROID_ABI=${ANDROID_ABI} DAEMON_DIR=${DAEMON_DIR} ${DAEMON_DIR}/bin/android/build-daemon.sh
)

find_library(log-lib log)
find_library(android-lib android)

include_directories(SYSTEM "${CONTRIB_DIR}/${TARGET_TUPLE_DRING}/include")
include_directories("${DAEMON_DIR}/src")

link_directories("${CONTRIB_DIR}/${TARGET_TUPLE_DRING}/lib")

list(APPEND staticLibs
    OpenSLES
    opendht
    pjsip-${TARGET_TUPLE_PJ}
    pjsip-simple-${TARGET_TUPLE_PJ}
    pjsip-ua-${TARGET_TUPLE_PJ}
    pjsua-${TARGET_TUPLE_PJ}
    pjnath-${TARGET_TUPLE_PJ}
    pjmedia-${TARGET_TUPLE_PJ}
    pjlib-util-${TARGET_TUPLE_PJ}
    pj-${TARGET_TUPLE_PJ}
    upnp ixml threadutil
    samplerate
    secp256k1
    gnutls nettle hogweed gmp
    restbed ssl crypto
    avformat avdevice avcodec avfilter avutil
    pcre
    sndfile
    yaml-cpp jsoncpp
    uuid z swscale
    vpx x264 opus speex vorbis vorbisenc ogg FLAC argon2
    iconv)

add_library(ring SHARED ${JNI_DIR}/ring_wrapper.cpp)
add_dependencies(ring dring)
target_link_libraries(ring ${DAEMON_LIB} ${log-lib} ${android-lib} ${staticLibs})
