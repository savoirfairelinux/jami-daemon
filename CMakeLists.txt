cmake_minimum_required(VERSION 3.16)

project(jami C CXX)

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

set(TARGET ${TARGET})
set(CONTRIB_PATH ${CMAKE_CURRENT_SOURCE_DIR}/contrib/${TARGET})
set(JAMI_DATADIR ${JAMI_DATADIR})
set(PACKAGE_NAME "Jami Daemon")

option(JAMI_NATPNP "Build Jami with NAT-PMP (if found)" ON)
option(JAMI_PUPNP "Build Jami with PUPNP (if found)" ON)
option(JAMI_PLUGIN "Build Jami with plugin support" ON)
option(JAMI_JNI "Build the Jami JNI binding" OFF)

message(CONTRIB_PATH="${CONTRIB_PATH}")

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
if (JAMI_NATPNP)
    list (APPEND ALL_FILES ${Source_Files__upnp__protocol__natpmp})
endif()
if (JAMI_PUPNP)
    list (APPEND ALL_FILES ${Source_Files__upnp__protocol__pupnp})
endif()
if (JAMI_PLUGIN)
    list (APPEND ALL_FILES ${Source_Files__plugin})
endif()

if(MSVC)
   list (APPEND ALL_FILES ${ALL_FILES}
         "${Source_Files__media__audio__portaudio}"
         "${Source_Files__media__video__uwpvideo}"
         "${Source_Files__media__video__winvideo}"
   )

   ################################################################################
   # Target
   ################################################################################
   include("CMake/Utils.cmake")

   add_library(${PROJECT_NAME} STATIC ${ALL_FILES})
   set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "daemon")

   use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")

   set_target_properties (${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)

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
           "${CMAKE_CURRENT_SOURCE_DIR}/.;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/client;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/config;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/security;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/sip;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/upnp;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/upnp/igd;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/upnp/protocol;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/upnp/mapping;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/jami;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/hooks;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/im;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/media;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/jamidht;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/jamidht/eth;"
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
           "ASIO_STANDALONE;"
           "STATIC_GETOPT;"
           "LIBARCHIVE_STATIC;"
           "OPENDHT_PROXY_CLIENT;"
           "OPENDHT_PROXY_SERVER;"
           "OPENDHT_PUSH_NOTIFICATIONS;"
           "NATPMP_STATICLIB;"
           "ENABLE_PLUGIN;"
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
           "MSGPACK_NO_BOOST;"
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
    list(APPEND CMAKE_FIND_ROOT_PATH ${CONTRIB_PATH})
    set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
    list(APPEND CMAKE_PREFIX_PATH ${CONTRIB_PATH})
    include(FindPkgConfig)

    #find_package (msgpack REQUIRED)
    find_package (jsoncpp REQUIRED)
    find_package (yaml-cpp REQUIRED)
    find_package (fmt REQUIRED)
    pkg_search_module (webrtcap REQUIRED IMPORTED_TARGET webrtc-audio-processing)
    pkg_search_module (gnutls REQUIRED IMPORTED_TARGET gnutls)
    pkg_search_module (opendht REQUIRED IMPORTED_TARGET opendht)
    pkg_search_module (git2 REQUIRED IMPORTED_TARGET git2 libgit2)
    pkg_search_module (archive REQUIRED IMPORTED_TARGET archive libarchive)
    pkg_search_module (secp256k1 REQUIRED IMPORTED_TARGET secp256k1 libsecp256k1)
    pkg_search_module (pjproject REQUIRED IMPORTED_TARGET pjproject libpjproject)
    pkg_search_module (avformat REQUIRED IMPORTED_TARGET avformat libavformat)
    pkg_search_module (avdevice REQUIRED IMPORTED_TARGET avdevice libavdevice)
    pkg_search_module (avfilter REQUIRED IMPORTED_TARGET avfilter libavfilter)
    pkg_search_module (avcodec REQUIRED IMPORTED_TARGET avcodec libavcodec)
    pkg_search_module (swresample REQUIRED IMPORTED_TARGET swresample libswresample)
    pkg_search_module (swscale REQUIRED IMPORTED_TARGET swscale libswscale)
    pkg_search_module (avutil REQUIRED IMPORTED_TARGET avutil libavutil)

    if (ANDROID)
        list (APPEND ALL_FILES ${ALL_FILES}
            ${CMAKE_CURRENT_SOURCE_DIR}/src/media/video/androidvideo/video_device_impl.cpp
            ${CMAKE_CURRENT_SOURCE_DIR}/src/media/video/androidvideo/video_device_monitor_impl.cpp
        )
    endif()
    add_library(${PROJECT_NAME} STATIC ${ALL_FILES})
    set_target_properties(${PROJECT_NAME} PROPERTIES FOLDER "daemon")
    set_target_properties(${PROJECT_NAME} PROPERTIES CXX_STANDARD 17)
    target_include_directories(${PROJECT_NAME} PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/.;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/client;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/config;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/security;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/sip;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/upnp;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/upnp/igd;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/upnp/protocol;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/connectivity/upnp/mapping;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/jami;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/hooks;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/im;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/media;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/jamidht;"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/jamidht/eth;"
        SYSTEM
        ${CONTRIB_PATH}/include
    )
    target_compile_options(${PROJECT_NAME} PRIVATE
        -DHAVE_RINGNS
        -DENABLE_VIDEO
        -DJAMI_DATADIR="${JAMI_DATADIR}"
        -DPACKAGE_NAME="${PACKAGE_NAME}"
        -DPACKAGE="jami"
        -DPJ_AUTOCONF=1
        -DASIO_STANDALONE
        -DMSGPACK_NO_BOOST
        -Wall -Wextra
        -Wnon-virtual-dtor
        -Wno-deprecated
        -Wno-deprecated-declarations
        -Wno-unknown-pragmas
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE
        jsoncpp
        yaml-cpp
        fmt
        secp256k1
        PkgConfig::webrtcap
        PkgConfig::gnutls
        PkgConfig::opendht
        PkgConfig::git2
        PkgConfig::archive
        PkgConfig::pjproject
        PkgConfig::avformat
        PkgConfig::avdevice
        PkgConfig::avfilter
        PkgConfig::avcodec
        PkgConfig::swresample
        PkgConfig::swscale
        PkgConfig::avutil
    )

    if (JAMI_NATPNP)
        pkg_search_module (natpmp IMPORTED_TARGET natpmp)
        target_link_libraries(${PROJECT_NAME} PRIVATE natpmp)
    endif()
    if (JAMI_PUPNP)
        pkg_search_module (upnp IMPORTED_TARGET upnp)
        target_link_libraries(${PROJECT_NAME} PRIVATE upnp)
    endif()
    if (JAMI_PLUGIN)
        target_compile_options(${PROJECT_NAME} PRIVATE -DENABLE_PLUGIN)
    endif()

    if (JAMI_JNI)
        add_library(${PROJECT_NAME}-jni SHARED ${CMAKE_CURRENT_SOURCE_DIR}/bin/jni/jami_wrapper.cpp)
        target_link_directories(${PROJECT_NAME}-jni PRIVATE ${CONTRIB_PATH}/lib)
        target_link_libraries(${PROJECT_NAME}-jni ${PROJECT_NAME} android log)
        set_target_properties(${PROJECT_NAME}-jni PROPERTIES CXX_STANDARD 17)
        target_link_options(${PROJECT_NAME}-jni PRIVATE
            -Wl,-Bsymbolic
            -Wall -Wextra
        )
    endif()

endif()
