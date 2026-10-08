vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO savoirfairelinux/opendht
    REF v4.4.1
    SHA512 b2934affb2547d38b557f7f7e74ebdd33fbca0d0537c12e050350ecd0c8ae067ad82e8299c1adb9627db77b3d6e4124d983deb14166505084bd5c477b81c5f8d
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
