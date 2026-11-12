vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

set(DHTNET_REF 1f1e277ef4af292d660e5c2e45be8ce5a9f3ef49)
vcpkg_download_distfile(ARCHIVE
    URLS "https://git.jami.net/savoirfairelinux/dhtnet/-/archive/${DHTNET_REF}/dhtnet-${DHTNET_REF}.tar.gz"
    FILENAME "dhtnet-${DHTNET_REF}.tar.gz"
    SHA512 380e18a03dec9b0bdfc021395da94a73b6e1339e83a1fd1a1cfe182e017d43272b16a5d60da9b9db0bec4aa45ee6aa10227ee76767d7e062e6b18b34c76f203c
)
vcpkg_extract_source_archive(SOURCE_PATH
    ARCHIVE "${ARCHIVE}"
    PATCHES
        use-pkgconfig-with-vcpkg.patch
        msvc-no-unistd.patch
        "${CMAKE_CURRENT_LIST_DIR}/../../../src/dhtnet/0001-interrupt-DTLS-handshake-on-shutdown.patch"
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
