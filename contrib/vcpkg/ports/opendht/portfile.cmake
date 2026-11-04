vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO savoirfairelinux/opendht
    REF v4.4.99
    SHA512 5224b3c48979b42a93e1ad44d57263729ae202a9f710dfb6ede1c981305bb98078e6b576f685f69f81583261dd35f32cc071814fbed18c8498c642fe83edd4d9
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
