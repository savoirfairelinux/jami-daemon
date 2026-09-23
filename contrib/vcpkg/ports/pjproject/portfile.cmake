vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO savoirfairelinux/pjproject
    REF 48a63353a378e88a169551e265f33eae59ebd26c
    SHA512 b5630241f996b49636bab38da6c9842d7db2143c87a6036530be4b3ffc824b7061b87797a64c5340c72d57b76c34f68ccba4747e13aaabfa5b2101a057e7c92f
    HEAD_REF master
)

# Same settings contrib/src/pjproject/000{1,2}-win-*.patch add to config_site.h
# (minus THIRD_PARTY_MEDIA, unused outside pjsip-apps). The include/lib dir
# injection of 0002 is done by USE_VCPKG_INTEGRATION.
file(APPEND "${SOURCE_PATH}/pjlib/include/pj/config_site.h" [[

/*
 * WINDOWS settings (Jami).
 */
#define PJ_OS_HAS_CHECK_STACK                   0
#define PJ_HAS_SSL_SOCK                         1
#define PJ_SSL_SOCK_IMP                         PJ_SSL_SOCK_IMP_GNUTLS
]])

# The autolink pragma names pywinmake's libgnutls.lib; vcpkg's is gnutls.lib and
# consumers get it from pkg-config (libpjproject.pc Requires.private: gnutls).
vcpkg_replace_string("${SOURCE_PATH}/pjlib/src/pj/ssl_sock_gtls.c"
    [[#  pragma comment( lib, "libgnutls")]] "")

set(PJ_PROJECTS pjlib pjlib_util pjnath pjmedia pjmedia_codec pjsip_core pjsip_simple pjsip_ua pjsua_lib pjsua2_lib)
list(JOIN PJ_PROJECTS "," PJ_TARGETS)

vcpkg_install_msbuild(
    USE_VCPKG_INTEGRATION
    SOURCE_PATH "${SOURCE_PATH}"
    PROJECT_SUBPATH pjproject-vs14.sln
    TARGET "${PJ_TARGETS}"
    PLATFORM ${TRIPLET_SYSTEM_ARCH}
    RELEASE_CONFIGURATION Release
    DEBUG_CONFIGURATION Debug
    LICENSE_SUBPATH COPYING
    SKIP_CLEAN
)

# pjlib-x86_64-x64-vc14-Release.lib -> pjlib.lib
file(GLOB libs "${CURRENT_PACKAGES_DIR}/lib/*.lib" "${CURRENT_PACKAGES_DIR}/debug/lib/*.lib")
foreach(lib IN LISTS libs)
    get_filename_component(dir "${lib}" DIRECTORY)
    get_filename_component(name "${lib}" NAME)
    string(REGEX REPLACE "-[^-]+-[^-]+-vc[0-9]+-[^-]+\\.lib$" ".lib" newname "${name}")
    if(NOT name STREQUAL newname)
        file(RENAME "${lib}" "${dir}/${newname}")
    endif()
endforeach()

foreach(mod IN ITEMS pjlib pjlib-util pjnath pjmedia pjsip)
    file(COPY "${SOURCE_PATH}/${mod}/include/" DESTINATION "${CURRENT_PACKAGES_DIR}/include")
endforeach()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/include/pj/compat/os_auto.h.in")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/include/pj/config_site_sample.h.orig")

file(STRINGS "${SOURCE_PATH}/version.mak" ver_lines REGEX "^export PJ_VERSION_(MAJOR|MINOR|REV) *:=")
foreach(line IN LISTS ver_lines)
    string(REGEX MATCH "PJ_VERSION_([A-Z]+) *:= *([0-9]+)" _ "${line}")
    set(PJ_VERSION_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}")
endforeach()

set(PJ_LIBS "-lpjsua2-lib -lpjsua-lib -lpjsip-ua -lpjsip-simple -lpjsip-core -lpjmedia-codec -lpjmedia -lpjnath -lpjlib-util -lpjlib")
set(PJ_SYSLIBS "-lws2_32 -liphlpapi -lole32 -ldsound -ldxguid -lnetapi32 -lmswsock -lwinmm -lodbc32 -lodbccp32")
foreach(cfg IN ITEMS "" "/debug")
    if(cfg AND VCPKG_BUILD_TYPE STREQUAL "release")
        continue()
    endif()
    file(MAKE_DIRECTORY "${CURRENT_PACKAGES_DIR}${cfg}/lib/pkgconfig")
    file(WRITE "${CURRENT_PACKAGES_DIR}${cfg}/lib/pkgconfig/libpjproject.pc" "\
prefix=\${pcfiledir}/../..
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: libpjproject
Description: Multimedia communication library
URL: http://www.pjsip.org
Version: ${PJ_VERSION_MAJOR}.${PJ_VERSION_MINOR}.${PJ_VERSION_REV}
Requires.private: gnutls
Libs: -L\${libdir} ${PJ_LIBS}
Libs.private: ${PJ_SYSLIBS}
Cflags: -I\${includedir}
")
endforeach()
vcpkg_fixup_pkgconfig()
