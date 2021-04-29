cmake_minimum_required(VERSION 3.15)

project(ring-daemon C CXX)

################################################################################
# Source groups
################################################################################

# compat, src source group

add_subdirectory("compat")
add_subdirectory("src")

source_group("Source Files" FILES ${Source_Files})
source_group("Source Files\\client" FILES ${Source_Files__client})
source_group("Source Files\\config" FILES ${Source_Files__config})
source_group("Source Files\\dring" FILES ${Source_Files__dring})
source_group("Source Files\\hooks" FILES ${Source_Files__hooks})
source_group("Source Files\\im" FILES ${Source_Files__im})
source_group("Source Files\\jamidht" FILES ${Source_Files__jamidht})
source_group("Source Files\\jamidht\\eth\\libdevcore" FILES ${Source_Files__jamidht__eth__libdevcore})
source_group("Source Files\\jamidht\\eth\\libdevcrypto" FILES ${Source_Files__jamidht__eth__libdevcrypto})
source_group("Source Files\\media" FILES ${Source_Files__media})
source_group("Source Files\\media\\audio" FILES ${Source_Files__media__audio})
source_group("Source Files\\media\\audio\\echo-cancel" FILES ${Source_Files__media__audio__echo_cancel})
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

source_group("Source Files\\security" FILES ${Source_Files__security})
source_group("Source Files\\sip" FILES ${Source_Files__sip})
source_group("Source Files\\upnp" FILES ${Source_Files__upnp})
source_group("Source Files\\upnp\\protocol" FILES ${Source_Files__upnp__protocol})
source_group("Source Files\\upnp\\protocol\\natpmp" FILES ${Source_Files__upnp__protocol__natpmp})
source_group("Source Files\\upnp\\protocol\\pupnp" FILES ${Source_Files__upnp__protocol__pupnp})

list (APPEND ALL_FILES
      ${Source_Files}
      ${Source_Files__client}
      ${Source_Files__config}
      ${Source_Files__dring}
      ${Source_Files__hooks}
      ${Source_Files__im}
      ${Source_Files__jamidht}
      ${Source_Files__jamidht__eth__libdevcore}
      ${Source_Files__jamidht__eth__libdevcrypto}
      ${Source_Files__media}
      ${Source_Files__media__audio}
      ${Source_Files__media__audio__sound}
      ${Source_Files__media__audio__echo_cancel}
      ${Source_Files__media__video}
      ${Source_Files__security}
      ${Source_Files__sip}
      ${Source_Files__upnp}
      ${Source_Files__upnp__protocol}
      ${Source_Files__upnp__protocol__natpmp}
      ${Source_Files__upnp__protocol__pupnp}
      ${Source_Files__plugin}
)

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
           TARGET_NAME_RELEASELIB_WIN32 "dring"
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
           "${CMAKE_CURRENT_SOURCE_DIR}/src/dring;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/hooks;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/im;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/media;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/jamidht;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/security;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/sip;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/upnp;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/upnp/igd;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/upnp/protocol;"
           "${CMAKE_CURRENT_SOURCE_DIR}/src/upnp/mapping;"
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
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/natpmp"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/msgpack-c/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/opendht/include;"
           "${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/minizip;"
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
           "MINIZIP;"
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
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/bin/avcodec.lib 
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/bin/avdevice.lib 
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/bin/avfilter.lib 
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/bin/avformat.lib 
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/bin/avutil.lib 
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/bin/swresample.lib 
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/ffmpeg/Build/win32/x64/bin/swscale.lib 
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/libgnutls.lib 
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/lib_json.lib 
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/opendht/build/Release/libopendht.lib 
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/argon2/vs2015/Argon2Ref/vs2015/build/Argon2Ref.lib 
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/secp256k1.lib 
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/yaml-cpp/msvc/Release/libyaml-cppmd.lib 
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/libupnp.lib 
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/build/natpmp/msvc/Release/natpmp.lib 
        ${CMAKE_CURRENT_SOURCE_DIR}/contrib/msvc/lib/x64/minizip.lib 
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

endif()