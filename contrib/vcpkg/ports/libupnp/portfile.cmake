vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO pupnp/pupnp
    REF "release-${VERSION}"
    SHA512 a4ffb5977d9e103f8ce3ed8f9dec6a336424258c9541ae38790accaa35cd7bf92165cded285ce30f57c7aa5e03f05f0a639d846436b460800ee5e5fcb4d2d7c8
    PATCHES
        fix-pthreads4w-targets.patch
)

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "static" LIBUPNP_BUILD_STATIC)
string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" LIBUPNP_BUILD_SHARED)

if(LIBUPNP_BUILD_STATIC)
    set(UPNP_TARGET "Static")
else()
    set(UPNP_TARGET "Shared")
endif()

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
    client-api      UPNP_ENABLE_CLIENT_API
    ipv6            UPNP_ENABLE_IPV6
    ssl             UPNP_ENABLE_OPEN_SSL
    webserver       UPNP_ENABLE_WEBSERVER
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
    -DBUILD_TESTING=OFF
    -DIXML_ENABLE_SCRIPT_SUPPORT=ON
    -DUPNP_ENABLE_GENA=ON
    -DUPNP_ENABLE_SOAP=ON
    -DUPNP_ENABLE_SSDP=ON
    -DUPNP_ENABLE_BLOCKING_TCP_CONNECTIONS=OFF
    -DUPNP_ENABLE_DEVICE_API=${UPNP_ENABLE_WEBSERVER}
    -DUPNP_MINISERVER_REUSEADDR=${UPNP_ENABLE_WEBSERVER}
    -DUPNP_BUILD_SHARED=${LIBUPNP_BUILD_SHARED}
    -DUPNP_BUILD_STATIC=${LIBUPNP_BUILD_STATIC}
    -DUPNP_BUILD_SAMPLES=OFF
    -DUPNP_ENABLE_UNSPECIFIED_SERVER=OFF
    ${FEATURE_OPTIONS}
)

vcpkg_cmake_install()

vcpkg_fixup_pkgconfig()

# Jami: upstream libupnp.pc.in hard-codes -lupnp -lixml and has no static
# define or pthreads. With MSVC static libs are named libupnps/ixmls and headers
# default to dllimport, so pkg-config consumers (dhtnet, libjami) cannot link.
if(VCPKG_TARGET_IS_WINDOWS AND LIBUPNP_BUILD_STATIC)
    foreach(cfg IN ITEMS "" "debug/")
        set(pc "${CURRENT_PACKAGES_DIR}/${cfg}lib/pkgconfig/libupnp.pc")
        if(EXISTS "${pc}")
            if(cfg)
                set(pthread_lib pthreadVC3d)
            else()
                set(pthread_lib pthreadVC3)
            endif()
            vcpkg_replace_string("${pc}" "-lupnp -lixml" "-llibupnps -lixmls -l${pthread_lib} -lws2_32 -liphlpapi")
            vcpkg_replace_string("${pc}" "Cflags: " "Cflags: -DUPNP_STATIC_LIB ")
        endif()
    endforeach()
endif()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/UPNP DO_NOT_DELETE_PARENT_CONFIG_PATH)
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/IXML)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

vcpkg_copy_pdbs()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")

file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")
