vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

set(DHTNET_REF 818ed67e967416cb10b8d7c5750198537e5ab957)
vcpkg_download_distfile(ARCHIVE
    URLS "https://git.jami.net/savoirfairelinux/dhtnet/-/archive/${DHTNET_REF}/dhtnet-${DHTNET_REF}.tar.gz"
    FILENAME "dhtnet-${DHTNET_REF}.tar.gz"
    SHA512 377c2ce742cb4892831a8b2f23886f2476e106a3ddcc760477481620581d488d1111111695f56acd9c9a6cc9334a71eb598d0479fa60c00bb4b297286a133ba9
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
