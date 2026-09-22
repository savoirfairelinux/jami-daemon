vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO savoirfairelinux/opendht
    REF a6e0e441ac002e97b411024928a6ac5185a65c40
    SHA512 aa926f5499e65cbf68f251b1febc5c3a92fca77089f8f067ef3c95a04635603c64bb0b7e469b6e8a9166688b110b8d9f405d16cbeb1c94e130dbfaf2eef31723
    HEAD_REF master
)

vcpkg_check_features(OUT_FEATURE_OPTIONS FEATURE_OPTIONS
    FEATURES
        proxy OPENDHT_PROXY_CLIENT
        proxy OPENDHT_PROXY_SERVER
        proxy OPENDHT_PUSH_NOTIFICATIONS
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        ${FEATURE_OPTIONS}
        -DCMAKE_CXX_STANDARD=20
        -DOPENDHT_DOWNLOAD_DEPS=OFF
        -DOPENDHT_USE_PKGCONFIG=OFF
        -DOPENDHT_TOOLS=OFF
        -DOPENDHT_PYTHON=OFF
        -DOPENDHT_C=OFF
        -DOPENDHT_DOCUMENTATION=OFF
        -DOPENDHT_IO_URING=OFF
        -DBUILD_TESTING=OFF
)

vcpkg_cmake_install()
vcpkg_cmake_config_fixup(CONFIG_PATH lib/cmake/opendht)
vcpkg_fixup_pkgconfig()
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include" "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
