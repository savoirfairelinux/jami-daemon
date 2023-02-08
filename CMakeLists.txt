cmake_minimum_required(VERSION 3.16)

project(jami-daemon
    VERSION 13.7.0
    LANGUAGES C CXX)
set(PACKAGE_NAME "Jami Daemon")

################################################################################
# Build options
################################################################################
option(JAMI_NATPMP "Build with NAT-PMP" ON)
option(JAMI_PUPNP "Build with PUPNP" ON)
option(JAMI_PLUGIN "Build with plugin support" ON)
option(JAMI_JNI "Build the JNI binding" OFF)
option(JAMI_DBUS "Build the DBUS binding" OFF)
option(JAMI_VIDEO "Build with video support" ON)
option(BUILD_CONTRIB "Build contrib to CONTRIB_PATH" ON)
option(BUILD_EXTRA_TOOLS "Build extra tools" OFF)

include(CTest)

if(NOT MSVC)
    ################################################################################
    # Contrib
    ################################################################################

    # Android target mapping for contrib
    if (ANDROID_ABI STREQUAL "x86")
        set(TARGET i686-linux-android)
    elseif (ANDROID_ABI STREQUAL "x86_64")
        set(TARGET x86_64-linux-android)
    elseif (ANDROID_ABI STREQUAL "arm64-v8a")
        set(TARGET aarch64-linux-android)
    elseif (ANDROID_ABI STREQUAL "armeabi-v7a")
        set(TARGET armv7a-linux-androideabi)
    else()
        set(TARGET ${CMAKE_LIBRARY_ARCHITECTURE})
        if (NOT TARGET)
            execute_process(COMMAND gcc -dumpmachine  COMMAND tr -d '\n' OUTPUT_VARIABLE TARGET)
        endif()
    endif()
    set(CONTRIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/contrib/${TARGET})

    include(ProcessorCount)
    ProcessorCount(NPROC)
    if(NOT NPROC EQUAL 0)
        set(CONTRIB_BUILD_FLAGS -j${NPROC})
    endif()

    if (BUILD_EXTRA_TOOLS)
        set(ENV{PATH} "${CMAKE_CURRENT_SOURCE_DIR}/extras/tools/build/bin:$ENV{PATH}")
        execute_process(
            COMMAND ./bootstrap
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/extras/tools)
        execute_process(
            COMMAND make ${CONTRIB_BUILD_FLAGS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/extras/tools)
        execute_process(
            COMMAND make .pkg-config
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/extras/tools)
        execute_process(
            COMMAND make .gas
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/extras/tools)
    endif()
    if (JAMI_JNI)
        # Generate swig binding
        add_custom_command(
            OUTPUT ${CMAKE_CURRENT_SOURCE_DIR}/bin/jni/jami_wrapper.cpp
            COMMAND env PACKAGEDIR=${JAMI_JNI_PACKAGEDIR} "./make-swig.sh"
            DEPENDS
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/jni/make-swig.sh
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/jni/jni_interface.i
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/jni/callmanager.i
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/jni/configurationmanager.i
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/jni/conversation.i
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/jni/managerimpl.i
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/jni/data_view.i
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/jni
        )
    endif()
    if (BUILD_CONTRIB)
        set(CONTRIB_BUILD_PATH ${CMAKE_CURRENT_SOURCE_DIR}/contrib/native-${TARGET})
        file(MAKE_DIRECTORY ${CONTRIB_BUILD_PATH})
        file(MAKE_DIRECTORY ${CONTRIB_PATH})

        if (ANDROID)
            # See https://developer.android.com/ndk/guides/other_build_systems
            set(ENV{ANDROID_NDK} ${ANDROID_NDK})
            set(ENV{ANDROID_ABI} ${ANDROID_ABI})
            set(ENV{ANDROID_API} ${ANDROID_PLATFORM})
            set(ENV{TOOLCHAIN} ${ANDROID_TOOLCHAIN_ROOT})
            set(ENV{TARGET} ${TARGET})
            set(ENV{API} ${ANDROID_PLATFORM_LEVEL})
            set(ENV{CC} $ENV{TOOLCHAIN}/bin/${TARGET}$ENV{API}-clang)
            set(ENV{CXX} $ENV{TOOLCHAIN}/bin/${TARGET}$ENV{API}-clang++)
            set(ENV{AS} "$ENV{CC} -c")
            set(ENV{AR} ${CMAKE_AR})
            set(ENV{RANLIB} ${CMAKE_RANLIB})
            set(ENV{STRIP} ${CMAKE_STRIP})
            set(ENV{LD} $ENV{TOOLCHAIN}/bin/ld)
        endif()

        execute_process(
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/contrib/bootstrap --host=${TARGET}
            WORKING_DIRECTORY ${CONTRIB_BUILD_PATH})
        execute_process(COMMAND make list
            WORKING_DIRECTORY ${CONTRIB_BUILD_PATH})
        execute_process(COMMAND make ${CONTRIB_BUILD_FLAGS}
            WORKING_DIRECTORY ${CONTRIB_BUILD_PATH})
    endif()
endif()

################################################################################
# Check dependencies
################################################################################

if(NOT MSVC)
    include(FindPkgConfig)
    if (JAMI_NATPMP)
        pkg_search_module (natpmp IMPORTED_TARGET natpmp)
        if (NOT natpmp_FOUND)
            find_library(natpmp_LIBRARIES natpmp)
            if (NOT natpmp_LIBRARIES)
                message("NAT-PMP not found: disabling")
                set(JAMI_NATPMP Off)
            endif()
        endif()
    endif()
    if (JAMI_PUPNP)
        pkg_search_module (upnp IMPORTED_TARGET upnp libupnp)
        if (NOT upnp_FOUND)
            set(JAMI_PUPNP Off)
        endif()
    endif()
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        pkg_search_module (udev REQUIRED IMPORTED_TARGET libudev)
        pkg_search_module (pulseaudio REQUIRED IMPORTED_TARGET libpulse)
    endif()
endif()

################################################################################
# Source groups
################################################################################

# compat, src source group
if(MSVC)
    add_subdirectory("compat")
endif()

add_subdirectory("src")
source_group("Source Files" FILES ${Source_Files})
source_group("Source Files\\client" FILES ${Source_Files__client})
source_group("Source Files\\config" FILES ${Source_Files__config})
source_group("Source Files\\connectivity" FILES ${Source_Files__connectivity})
source_group("Source Files\\connectivity\\security" FILES ${Source_Files__security})
source_group("Source Files\\connectivity\\sip" FILES ${Source_Files__sip})
source_group("Source Files\\connectivity\\upnp" FILES ${Source_Files__upnp})
source_group("Source Files\\connectivity\\upnp\\protocol" FILES ${Source_Files__upnp__protocol})
source_group("Source Files\\connectivity\\upnp\\protocol\\natpmp" FILES ${Source_Files__upnp__protocol__natpmp})
source_group("Source Files\\connectivity\\upnp\\protocol\\pupnp" FILES ${Source_Files__upnp__protocol__pupnp})
source_group("Source Files\\jami" FILES ${Source_Files__jami})
source_group("Source Files\\hooks" FILES ${Source_Files__hooks})
source_group("Source Files\\im" FILES ${Source_Files__im})
source_group("Source Files\\jamidht" FILES ${Source_Files__jamidht})
source_group("Source Files\\jamidht\\eth\\libdevcore" FILES ${Source_Files__jamidht__eth__libdevcore})
source_group("Source Files\\jamidht\\eth\\libdevcrypto" FILES ${Source_Files__jamidht__eth__libdevcrypto})
source_group("Source Files\\media" FILES ${Source_Files__media})
source_group("Source Files\\media\\audio" FILES ${Source_Files__media__audio})
source_group("Source Files\\media\\audio\\audio-processing" FILES ${Source_Files__media__audio__audio_processing})
source_group("Source Files\\media\\audio\\sound" FILES ${Source_Files__media__audio__sound})
source_group("Source Files\\media\\video" FILES ${Source_Files__media__video})
source_group("Source Files\\plugin" FILES ${Source_Files__plugin})

if(MSVC)
   if(WINDOWS_STORE)
      source_group("Source Files\\media\\video\\uwpvideo" FILES ${Source_Files__media__video__uwpvideo})
   endif()
   source_group("Source Files\\media\\audio\\portaudio" FILES ${Source_Files__media__audio__portaudio})
   source_group("Source Files\\media\\video\\winvideo" FILES ${Source_Files__media__video__winvideo})
endif()

list (APPEND ALL_FILES
      ${Source_Files}
      ${Source_Files__client}
      ${Source_Files__config}
      ${Source_Files__connectivity}
      ${Source_Files__jami}
      ${Source_Files__hooks}
      ${Source_Files__im}
      ${Source_Files__jamidht}
      ${Source_Files__jamidht__eth__libdevcore}
      ${Source_Files__jamidht__eth__libdevcrypto}
      ${Source_Files__media}
      ${Source_Files__media__audio}
      ${Source_Files__media__audio__sound}
      ${Source_Files__media__audio__audio_processing}
      ${Source_Files__media__video}
      ${Source_Files__security}
      ${Source_Files__sip}
      ${Source_Files__upnp}
      ${Source_Files__upnp__protocol}
)
if (JAMI_NATPMP)
    list (APPEND ALL_FILES ${Source_Files__upnp__protocol__natpmp})
endif()
if (JAMI_PUPNP)
    list (APPEND ALL_FILES ${Source_Files__upnp__protocol__pupnp})
endif()
if (JAMI_PLUGIN)
    list (APPEND ALL_FILES ${Source_Files__plugin})
endif()
if (ANDROID)
    list (APPEND ALL_FILES ${ALL_FILES}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/media/video/androidvideo/video_device_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/media/video/androidvideo/video_device_monitor_impl.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/media/audio/opensl/audio_player.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/media/audio/opensl/audio_recorder.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/media/audio/opensl/opensllayer.cpp
    )
elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if (JAMI_VIDEO)
        list (APPEND ALL_FILES ${ALL_FILES}
            ${CMAKE_CURRENT_SOURCE_DIR}/src/media/video/v4l2/video_device_impl.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/media/video/v4l2/video_device_monitor_impl.cpp
        )
    endif()
    list (APPEND ALL_FILES ${ALL_FILES}
        ${CMAKE_CURRENT_SOURCE_DIR}/src/media/audio/pulseaudio/audiostream.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/media/audio/pulseaudio/pulselayer.cpp
    )
elseif(MSVC)
   list (APPEND ALL_FILES ${ALL_FILES}
         "${Source_Files__media__audio__portaudio}"
         "${Source_Files__media__video__uwpvideo}"
         "${Source_Files__media__video__winvideo}"
   )
endif()

add_library(${PROJECT_NAME} STATIC ${ALL_FILES})
set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "daemon")
set_target_properties (${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)
target_compile_definitions(${PROJECT_NAME} PRIVATE
    PACKAGE_NAME="${PACKAGE_NAME}"
    PACKAGE_VERSION="${PROJECT_VERSION}"
    PACKAGE="jami"
    LIBJAMI_BUILD
    HAVE_RINGNS
    ASIO_STANDALONE
    MSGPACK_NO_BOOST
)
if (JAMI_VIDEO)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_VIDEO)
endif()
if (JAMI_PLUGIN)
    target_compile_definitions(${PROJECT_NAME} PRIVATE ENABLE_PLUGIN)
endif()
if (JAMI_NATPMP)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_LIBNATPMP)
endif()
if (ANDROID)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_OPENSL)
endif()
target_include_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/.;"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/config;"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity;"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/security;"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/sip;"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/upnp;"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/upnp/igd;"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/upnp/protocol;"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/upnp/mapping;"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/client;"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/hooks;"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/im;"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/media;"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/jamidht;"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/jamidht/eth;"
    PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src;"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/jami;"
)

if(MSVC)
   ################################################################################
   # Target
   ################################################################################
   include("CMake/Utils.cmake")
   use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")

   ################################################################################
   # Include necessary .cmake file (need to init use_props first)
   ################################################################################
   include("CMake/Default.cmake")
   include("CMake/DefaultCXX.cmake")

   # Supress generation of ZERO_CHECK
   set(CMAKE_SUPPRESS_REGENERATION true)
   set(ROOT_NAMESPACE ringdaemon)

   ################################################################################
   # Target name
   ################################################################################
   if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
       set_target_properties(${PROJECT_NAME} PROPERTIES
           TARGET_NAME_RELEASELIB_WIN32 "jami"
       )
   endif()
   ################################################################################
   # Output directory
   ################################################################################
   if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
       set_target_properties(${PROJECT_NAME} PROPERTIES
           OUTPUT_DIRECTORY_RELEASELIB_WIN32 "${CMAKE_CURRENT_SOURCE_DIR}/build/${CMAKE_VS_PLATFORM_NAME}/$<CONFIG>/bin/"
       )
   endif()
   if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
       set_target_properties(${PROJECT_NAME} PROPERTIES
           INTERPROCEDURAL_OPTIMIZATION_RELEASELIB_WIN32 "FALSE"
       )
   endif()
   ################################################################################
   # Include directories
   ################################################################################
   if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
       target_include_directories(${PROJECT_NAME} PUBLIC
           "${CMAKE_CURRENT_SOURCE_DIR}/compat/msvc;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/include/upnp;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/sndfile/src;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/openssl/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/asio/asio/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/restinio/dev;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/fmt/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/http_parser;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/natpmp/include"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/msgpack-c/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/opendht/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/libarchive/libarchive;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/jsoncpp/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/yaml-cpp/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjlib/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjnath/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjlib-util/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjsip/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/third_party;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/speexdsp/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/webrtc-audio-processing;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/portaudio/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjmedia/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/libgit2/include"
       )
   endif()

   ################################################################################
   # Compile definitions
   ################################################################################
   if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
       target_compile_definitions(${PROJECT_NAME} PRIVATE
           "_WIN32_WINNT=0x0A00;"
           "STATIC_GETOPT;"
           "LIBARCHIVE_STATIC;"
           "OPENDHT_PROXY_CLIENT;"
           "OPENDHT_PROXY_SERVER;"
           "OPENDHT_PUSH_NOTIFICATIONS;"
           "NATPMP_STATICLIB;"
           "NDEBUG;"
           "_USE_MATH_DEFINES;"
           "_SCL_SECURE_NO_WARNINGS;"
           "_CRT_SECURE_NO_WARNINGS;"
           "NOMINMAX;"
           "HAVE_CONFIG_H;"
           "WIN32_LEAN_AND_MEAN;"
           "WEBRTC_WIN;"
           "WEBRTC_AUDIO_PROCESSING_ONLY_BUILD;"
           "WEBRTC_NS_FLOAT;"
       )
   endif()

   ################################################################################
   # Compile and link options
   ################################################################################
   if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
       target_compile_options(${PROJECT_NAME} PRIVATE
           /O2;
           /Oi;
           ${DEFAULT_CXX_RUNTIME_LIBRARY};
           /Gy;
           /MP;
           /Oy-;
           /sdl-;
           /W0;
           /FC;
           /FS
           /nologo;
           /Zi;
           /wd4996;
           /wd4503;
           /wd4180;
           /wd4244;
           /wd4267;
           /Zc:__cplusplus;
           ${DEFAULT_CXX_EXCEPTION_HANDLING}
       )
   endif()
   if("${CMAKE_VS_PLATFORM_NAME}" STREQUAL "x64")
       target_link_options(${PROJECT_NAME} PRIVATE
           /MACHINE:X64;
           /OPT:REF;
           /OPT:ICF
           /VERBOSE:LIB;
           /NODEFAULTLIB:libcmtd;
           /NODEFAULTLIB:libcmt;
           /OPT:NOLBR;
           /OPT:NOICF;
           /LTCG
       )
   endif()

   ################################################################################
   # Dependencies
   ################################################################################

   set(libAdditionalDependencies "${CMAKE_STATIC_LINKER_FLAGS} /LTCG
        ws2_32.lib
        advapi32.lib
        Secur32.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/lib/libavcodec.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/lib/libavdevice.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/lib/libavfilter.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/lib/libavformat.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/lib/libavutil.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/lib/libswresample.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/lib/libswscale.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/libx264.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/libvpx.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/libopus.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/libmfx.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/libgnutls.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/lib_json.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/opendht/build/Release/libopendht.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/argon2/vs2015/Argon2Ref/vs2015/build/Argon2Ref.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/secp256k1.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/yaml-cpp/msvc/Release/libyaml-cppmd.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/libupnp.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/natpmp/build/Release/natpmp.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/archive_static.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjsip/lib/pjsip-core-x86_64-x64-vc15-Release.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjsip/lib/pjsip-simple-x86_64-x64-vc15-Release.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjsip/lib/pjsua2-lib-x86_64-x64-vc15-Release.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjsip/lib/pjsua-lib-x86_64-x64-vc15-Release.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjsip/lib/pjsip-ua-x86_64-x64-vc15-Release.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjmedia/lib/pjmedia-x86_64-x64-vc15-Release.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjlib-util/lib/pjlib-util-x86_64-x64-vc15-Release.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjlib/lib/pjlib-x86_64-x64-vc15-Release.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/pjproject/pjnath/lib/pjnath-x86_64-x64-vc15-Release.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/fmt/msvc/Release/fmt.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/http_parser/x64/Release/http-parser.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/openssl/libcrypto.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/openssl/libssl.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/speexdsp/lib/libspeexdsp.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/webrtc-audio-processing/build/Release/webrtc-audio-processing.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/portaudio/build/Release/portaudio_static_x64.lib
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/libgit2/msvc/Release/git2.lib
        /ignore:4006"
   )

   set ( CMAKE_STATIC_LINKER_FLAGS ${libAdditionalDependencies} )
else()
    message("Contrib path: ${CONTRIB_PATH}")
    list(APPEND CMAKE_FIND_ROOT_PATH ${CONTRIB_PATH})
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
    list(APPEND CMAKE_PREFIX_PATH ${CONTRIB_PATH})

    find_package (jsoncpp REQUIRED)
    find_package (yaml-cpp REQUIRED)
    pkg_search_module (fmt REQUIRED IMPORTED_TARGET fmt)
    pkg_search_module (webrtcap REQUIRED IMPORTED_TARGET webrtc-audio-processing)
    pkg_search_module (gnutls REQUIRED IMPORTED_TARGET gnutls)
    pkg_search_module (opendht REQUIRED IMPORTED_TARGET opendht)
    pkg_search_module (git2 REQUIRED IMPORTED_TARGET libgit2)
    pkg_search_module (archive REQUIRED IMPORTED_TARGET libarchive)
    pkg_search_module (secp256k1 REQUIRED IMPORTED_TARGET libsecp256k1)
    pkg_search_module (pjproject REQUIRED IMPORTED_TARGET libpjproject)
    pkg_search_module (avformat REQUIRED IMPORTED_TARGET libavformat)
    pkg_search_module (avdevice REQUIRED IMPORTED_TARGET libavdevice)
    pkg_search_module (avfilter REQUIRED IMPORTED_TARGET libavfilter)
    pkg_search_module (avcodec REQUIRED IMPORTED_TARGET libavcodec)
    pkg_search_module (swresample REQUIRED IMPORTED_TARGET libswresample)
    pkg_search_module (swscale REQUIRED IMPORTED_TARGET libswscale)
    pkg_search_module (avutil REQUIRED IMPORTED_TARGET libavutil)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        PkgConfig::opendht
        PkgConfig::pjproject
        PkgConfig::git2
        PkgConfig::archive
        PkgConfig::gnutls
        PkgConfig::webrtcap
        PkgConfig::avformat
        PkgConfig::avdevice
        PkgConfig::avfilter
        PkgConfig::avcodec
        PkgConfig::swresample
        PkgConfig::swscale
        jsoncpp
        yaml-cpp
        PkgConfig::fmt
        PkgConfig::secp256k1
        PUBLIC
        PkgConfig::avutil
    )
    target_compile_options(${PROJECT_NAME} PRIVATE
        -fvisibility=hidden
        -Wall -Wextra
        -Wnon-virtual-dtor
        -Wno-deprecated
        -Wno-deprecated-declarations
        -Wno-unknown-pragmas
    )
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        JAMI_DATADIR="${JAMI_DATADIR}"
        PJ_AUTOCONF=1
    )
    if (ANDROID)
        target_link_libraries(${PROJECT_NAME} PRIVATE OpenSLES log)
    elseif (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_link_libraries(${PROJECT_NAME} PRIVATE
            PkgConfig::udev
            PkgConfig::pulseaudio
        )
    endif()
    if (JAMI_NATPMP)
        if (natpmp_FOUND)
            target_link_libraries(${PROJECT_NAME} PRIVATE PkgConfig::natpmp)
        elseif (natpmp_LIBRARIES)
            target_link_libraries(${PROJECT_NAME} PRIVATE ${natpmp_LIBRARIES})
        endif()
    endif()
    if (JAMI_PUPNP)
        target_link_libraries(${PROJECT_NAME} PRIVATE upnp)
    endif()

    if (JAMI_JNI)
        # Build jni binding
        add_library(${PROJECT_NAME}-jni SHARED ${CMAKE_CURRENT_SOURCE_DIR}/bin/jni/jami_wrapper.cpp)
        target_link_directories(${PROJECT_NAME}-jni PRIVATE ${CONTRIB_PATH}/lib)
        target_link_libraries(${PROJECT_NAME}-jni ${PROJECT_NAME} android log)
        set_target_properties(${PROJECT_NAME}-jni PROPERTIES CXX_STANDARD 17)
        target_compile_options(${PROJECT_NAME}-jni PRIVATE
            -fvisibility=hidden
            -Wall -Wextra
            -Wnon-virtual-dtor
            -Wno-deprecated
            -Wno-deprecated-declarations
            -Wno-unknown-pragmas
            -Wno-unused-function
            -Wno-unused-parameter
        )
        target_link_options(${PROJECT_NAME}-jni PRIVATE
            -Wl,-Bsymbolic
            -Wall -Wextra
        )
    endif()

    if (JAMI_DBUS)
        find_program(DBusXml2Cpp dbusxx-xml2cpp REQUIRED)
        pkg_search_module (DBusCpp REQUIRED IMPORTED_TARGET dbus-c++-1)
        configure_file(
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/net.jami.daemon.service.in
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/cx.ring.Ring.service
            @ONLY)
        set(DBUS_XML_FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/cx.ring.Ring.CallManager.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/cx.ring.Ring.ConfigurationManager.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/cx.ring.Ring.Instance.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/cx.ring.Ring.PresenceManager.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/cx.ring.Ring.VideoManager.xml
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/cx.ring.Ring.PluginManagerInterface.xml
        )
        set(DBUS_ADAPTOR_FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/dbuscallmanager.adaptor.h
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/dbusconfigurationmanager.adaptor.h
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/dbusinstance.adaptor.h
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/dbuspresencemanager.adaptor.h
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/dbusvideomanager.adaptor.h
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/dbuspluginmanagerinterface.adaptor.h
        )
        set(DBUS_FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/dbuscallmanager.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/dbusclient.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/dbusconfigurationmanager.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/dbusinstance.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/dbuspresencemanager.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/bin/dbus/main.cpp
        )

        # XML adaptors
        foreach(DBUS_XML_FILE ${DBUS_XML_FILES})
            list(FIND DBUS_XML_FILES ${DBUS_XML_FILE} DBUS_XML_FILE_INDEX)
            list(GET DBUS_ADAPTOR_FILES ${DBUS_XML_FILE_INDEX} DBUS_ADAPTOR_FILE)
            add_custom_command(
                OUTPUT ${DBUS_ADAPTOR_FILE}
                COMMAND ${DBusXml2Cpp} ${DBUS_XML_FILE} --adaptor=${DBUS_ADAPTOR_FILE}
                DEPENDS ${DBUS_XML_FILE}
            )
        endforeach()

        # Jamid
        add_executable(jamid
            ${DBUS_FILES}
            ${DBUS_ADAPTOR_FILES}
        )
        target_link_libraries(jamid ${PROJECT_NAME} PkgConfig::DBusCpp)
    endif()

    if (BUILD_TESTING)
        pkg_search_module(Cppunit REQUIRED IMPORTED_TARGET cppunit)

        add_library(ut_library STATIC test/unitTest/common.cpp)
        target_link_libraries(ut_library
            PUBLIC
            ${PROJECT_NAME}
            ${CMAKE_THREAD_LIBS_INIT}
            PkgConfig::Cppunit
            PkgConfig::opendht
            PkgConfig::pjproject
            PkgConfig::git2
            PkgConfig::archive
            PkgConfig::gnutls
            PkgConfig::webrtcap
            PkgConfig::avformat
            PkgConfig::avdevice
            PkgConfig::avfilter
            PkgConfig::avcodec
            PkgConfig::swresample
            PkgConfig::swscale
            jsoncpp
            yaml-cpp
            PkgConfig::fmt
            PkgConfig::secp256k1
        )
        target_include_directories(ut_library PUBLIC
            "${CMAKE_CURRENT_SOURCE_DIR}/test;"
            "${CMAKE_CURRENT_SOURCE_DIR}/test/unitTest;"
        )
        target_compile_options(ut_library PUBLIC
            -fvisibility=hidden
            -Wall -Wextra
            -Wnon-virtual-dtor
            -Wno-deprecated
            -Wno-deprecated-declarations
            -Wno-unknown-pragmas
        )
        target_compile_definitions(ut_library PUBLIC
            JAMI_DATADIR="${JAMI_DATADIR}"
            PJ_AUTOCONF=1
            HAVE_RINGNS
            ASIO_STANDALONE
            MSGPACK_NO_BOOST
        )
        if (JAMI_PLUGIN)
            target_compile_definitions(ut_library PUBLIC ENABLE_PLUGIN)
        endif()
        if (JAMI_VIDEO)
            target_compile_definitions(ut_library PUBLIC ENABLE_VIDEO)
        endif()

        add_executable(ut_conversationRepository test/unitTest/conversationRepository/conversationRepository.cpp)
        target_link_libraries(ut_conversationRepository ut_library)
        add_test(NAME conversationRepository COMMAND ut_conversationRepository)

        add_executable(ut_revoke test/unitTest/revoke/revoke.cpp)
        target_link_libraries(ut_revoke ut_library)
        add_test(NAME revoke COMMAND ut_revoke)

        add_executable(ut_syncHistory test/unitTest/syncHistory/syncHistory.cpp)
        target_link_libraries(ut_syncHistory ut_library)
        add_test(NAME syncHistory COMMAND ut_syncHistory)

        add_executable(ut_base64 test/unitTest/base64/base64.cpp)
        target_link_libraries(ut_base64 ut_library)
        add_test(NAME base64 COMMAND ut_base64)

        add_executable(ut_ice_media_cand_exchange test/unitTest/ice/ice_media_cand_exchange.cpp)
        target_link_libraries(ut_ice_media_cand_exchange ut_library)
        add_test(NAME ice_media_cand_exchange COMMAND ut_ice_media_cand_exchange)

        add_executable(ut_ice test/unitTest/ice/ice.cpp)
        target_link_libraries(ut_ice ut_library)
        add_test(NAME ice COMMAND ut_ice)

        add_executable(ut_ice_sdp_parser test/unitTest/ice/ice_sdp_parser.cpp)
        target_link_libraries(ut_ice_sdp_parser ut_library)
        add_test(NAME ice_sdp_parser COMMAND ut_ice_sdp_parser)

        add_executable(ut_scheduler test/unitTest/scheduler.cpp)
        target_link_libraries(ut_scheduler ut_library)
        add_test(NAME scheduler COMMAND ut_scheduler)

        add_executable(ut_testAccount_factory test/unitTest/account_factory/testAccount_factory.cpp)
        target_link_libraries(ut_testAccount_factory ut_library)
        add_test(NAME testAccount_factory COMMAND ut_testAccount_factory)

        add_executable(ut_call test/unitTest/call/call.cpp)
        target_link_libraries(ut_call ut_library)
        add_test(NAME call COMMAND ut_call)

        add_executable(ut_conference test/unitTest/call/conference.cpp)
        target_link_libraries(ut_conference ut_library)
        add_test(NAME conference COMMAND ut_conference)

        add_executable(ut_recorder test/unitTest/call/recorder.cpp)
        target_link_libraries(ut_recorder ut_library)
        add_test(NAME recorder COMMAND ut_recorder)

        add_executable(ut_connectionManager test/unitTest/connectionManager/connectionManager.cpp)
        target_link_libraries(ut_connectionManager ut_library)
        add_test(NAME connectionManager COMMAND ut_connectionManager)

        add_executable(ut_migration test/unitTest/account_archive/migration.cpp)
        target_link_libraries(ut_migration ut_library)
        add_test(NAME migration COMMAND ut_migration)

        add_executable(ut_accountArchive test/unitTest/account_archive/account_archive.cpp)
        target_link_libraries(ut_accountArchive ut_library)
        add_test(NAME accountArchive COMMAND ut_accountArchive)

        add_executable(ut_mapUtils test/unitTest/map_utils/testMap_utils.cpp)
        target_link_libraries(ut_mapUtils ut_library)
        add_test(NAME mapUtils COMMAND ut_mapUtils)

        add_executable(ut_sipBasicCalls test/unitTest/sip_account/sip_basic_calls.cpp)
        target_link_libraries(ut_sipBasicCalls ut_library)
        add_test(NAME sipBasicCalls COMMAND ut_sipBasicCalls)

        add_executable(ut_sipSrtp test/unitTest/sip_account/sip_srtp.cpp)
        target_link_libraries(ut_sipSrtp ut_library)
        add_test(NAME sipSrtp COMMAND ut_sipSrtp)

        add_executable(ut_fileTransfer test/unitTest/fileTransfer/fileTransfer.cpp)
        target_link_libraries(ut_fileTransfer ut_library)
        add_test(NAME fileTransfer COMMAND ut_fileTransfer)

        add_executable(ut_fileutils test/unitTest/fileutils/testFileutils.cpp)
        target_link_libraries(ut_fileutils ut_library)
        add_test(NAME fileutils COMMAND ut_fileutils)

        add_executable(ut_holdResume test/unitTest/media_negotiation/hold_resume.cpp)
        target_link_libraries(ut_holdResume ut_library)
        add_test(NAME holdResume COMMAND ut_holdResume)

        add_executable(ut_autoAnswer test/unitTest/media_negotiation/auto_answer.cpp)
        target_link_libraries(ut_autoAnswer ut_library)
        add_test(NAME autoAnswer COMMAND ut_autoAnswer)

        add_executable(ut_mediaNegotiation test/unitTest/media_negotiation/media_negotiation.cpp)
        target_link_libraries(ut_mediaNegotiation ut_library)
        add_test(NAME mediaNegotiation COMMAND ut_mediaNegotiation)

        add_executable(ut_stringUtils test/unitTest/string_utils/testString_utils.cpp)
        target_link_libraries(ut_stringUtils ut_library)
        add_test(NAME stringUtils COMMAND ut_stringUtils)

        add_executable(ut_utf8_utils test/unitTest/utf8_utils/testUtf8_utils.cpp)
        target_link_libraries(ut_utf8_utils ut_library)
        add_test(NAME utf8_utils COMMAND ut_utf8_utils)

        add_executable(ut_certstore test/unitTest/certstore.cpp)
        target_link_libraries(ut_certstore ut_library)
        add_test(NAME certstore COMMAND ut_certstore)

        add_executable(ut_conversationCall test/unitTest/conversation/call.cpp test/unitTest/conversation/conversationcommon.cpp)
        target_link_libraries(ut_conversationCall ut_library)
        add_test(NAME conversationCall COMMAND ut_conversationCall)

        add_executable(ut_conversation test/unitTest/conversation/conversation.cpp test/unitTest/conversation/conversationcommon.cpp)
        target_link_libraries(ut_conversation ut_library)
        add_test(NAME conversation COMMAND ut_conversation)

        add_executable(ut_conversationRequest test/unitTest/conversation/conversationRequest.cpp test/unitTest/conversation/conversationcommon.cpp)
        target_link_libraries(ut_conversationRequest ut_library)
        add_test(NAME conversationRequest COMMAND ut_conversationRequest)

        add_executable(ut_conversationMembersEvent test/unitTest/conversation/conversationMembersEvent.cpp test/unitTest/conversation/conversationcommon.cpp)
        target_link_libraries(ut_conversationMembersEvent ut_library)
        add_test(NAME conversationMembersEvent COMMAND ut_conversationMembersEvent)

        add_executable(ut_test_media_frame test/unitTest/media/test_media_frame.cpp)
        target_link_libraries(ut_test_media_frame ut_library)
        add_test(NAME test_media_frame COMMAND ut_test_media_frame)

        add_executable(ut_test_video_scaler test/unitTest/media/video/test_video_scaler.cpp)
        target_link_libraries(ut_test_video_scaler ut_library)
        add_test(NAME test_video_scaler COMMAND ut_test_video_scaler)

        add_executable(ut_testVideo_input test/unitTest/media/video/testVideo_input.cpp)
        target_link_libraries(ut_testVideo_input ut_library)
        add_test(NAME testVideo_input COMMAND ut_testVideo_input)

        add_executable(ut_test_media_encoder test/unitTest/media/test_media_encoder.cpp)
        target_link_libraries(ut_test_media_encoder ut_library)
        add_test(NAME test_media_encoder COMMAND ut_test_media_encoder)

        add_executable(ut_test_media_decoder test/unitTest/media/test_media_decoder.cpp)
        target_link_libraries(ut_test_media_decoder ut_library)
        add_test(NAME test_media_decoder COMMAND ut_test_media_decoder)

        add_executable(ut_test_resampler test/unitTest/media/audio/test_resampler.cpp)
        target_link_libraries(ut_test_resampler ut_library)
        add_test(NAME test_resampler COMMAND ut_test_resampler)

        add_executable(ut_test_audio_frame_resizer test/unitTest/media/audio/test_audio_frame_resizer.cpp)
        target_link_libraries(ut_test_audio_frame_resizer ut_library)
        add_test(NAME test_audio_frame_resizer COMMAND ut_test_audio_frame_resizer)

        add_executable(ut_test_media_filter test/unitTest/media/test_media_filter.cpp)
        target_link_libraries(ut_test_media_filter ut_library)
        add_test(NAME test_media_filter COMMAND ut_test_media_filter)

        add_executable(ut_test_media_player test/unitTest/media/test_media_player.cpp)
        target_link_libraries(ut_test_media_player ut_library)
        add_test(NAME test_media_player COMMAND ut_test_media_player)

        if (JAMI_PLUGIN)
            add_executable(ut_plugins test/unitTest/plugins/plugins.cpp)
            target_link_libraries(ut_plugins ut_library)
            add_test(NAME plugins COMMAND ut_plugins)
        endif()
    endif()

endif()
