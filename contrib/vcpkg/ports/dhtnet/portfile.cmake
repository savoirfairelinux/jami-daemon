vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

set(DHTNET_REF 7e6324ffdeaf19f4a2870f9f6f40857898817575)
vcpkg_download_distfile(ARCHIVE
    URLS "https://git.jami.net/savoirfairelinux/dhtnet/-/archive/${DHTNET_REF}/dhtnet-${DHTNET_REF}.tar.gz"
    FILENAME "dhtnet-${DHTNET_REF}.tar.gz"
    SHA512 be1a629ecc985799306506ce94b83c914ec40c13401372e0d737ba6d4ded9e52c9558dbd8437aed38dc10c43481ffa9a85bdd508cb3f98c8a9aff09a74f8ef62
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
