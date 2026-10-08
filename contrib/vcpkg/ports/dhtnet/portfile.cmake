vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

set(DHTNET_REF 01333a9db660309a8a8a71c589a66782d8bde125)
vcpkg_download_distfile(ARCHIVE
    URLS "https://git.jami.net/savoirfairelinux/dhtnet/-/archive/${DHTNET_REF}/dhtnet-${DHTNET_REF}.tar.gz"
    FILENAME "dhtnet-${DHTNET_REF}.tar.gz"
    SHA512 521267de070a7d599f46940713bc2bf3e2ff1115cfa4c29edce37b46d68704f5617629dc26c9cfbf41a2758af1180421010b41355997f9ffc3fff2437ae026a7
)
vcpkg_extract_source_archive(SOURCE_PATH
    ARCHIVE "${ARCHIVE}"
    PATCHES
        use-pkgconfig-with-vcpkg.patch
        msvc-no-unistd.patch
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        natpmp DHTNET_NATPMP
        upnp   DHTNET_PUPNP
)

vcpkg_find_acquire_program(PKGCONFIG)
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        "-DPKG_CONFIG_EXECUTABLE=${PKGCONFIG}"
        -DBUILD_DEPENDENCIES=OFF
        -DBUILD_TOOLS=OFF
        -DBUILD_TESTING=OFF
        -DBUILD_BENCHMARKS=OFF
        -DBUILD_EXAMPLE=OFF
        -DDNC_SYSTEMD=OFF
)
vcpkg_cmake_install()
vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
