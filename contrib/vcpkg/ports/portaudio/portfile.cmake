vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO PortAudio/portaudio
    REF 9abe5fe7db729280080a0bbc1397a528cd3ce658
    SHA512 9c4a1283290915bf8be13ac33514038e0a2af6a251eda8b619f8230e4749ac163bd0d6f3fdfa5f5fbed5c6c016a4b8e92eca325ba5edef4215ed456ac76f48ba
    HEAD_REF master
    PATCHES
        0001-add-get-default-comm-devices-api.patch
)

string(COMPARE EQUAL "${VCPKG_LIBRARY_LINKAGE}" "dynamic" PA_BUILD_SHARED_LIBS)

# Same host API selection as contrib/src/portaudio/package.json.
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DPA_BUILD_SHARED_LIBS=${PA_BUILD_SHARED_LIBS}
        -DPA_BUILD_TESTS=OFF
        -DPA_BUILD_EXAMPLES=OFF
        -DPA_USE_ASIO=OFF
        -DPA_USE_DS=OFF
        -DPA_USE_WMME=OFF
        -DPA_USE_WDMKS=OFF
        -DPA_USE_WASAPI=ON
    OPTIONS_DEBUG
        -DPA_ENABLE_DEBUG_OUTPUT=ON
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME PortAudio CONFIG_PATH lib/cmake/portaudio)
vcpkg_copy_pdbs()
vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE
    "${CURRENT_PACKAGES_DIR}/debug/include"
    "${CURRENT_PACKAGES_DIR}/debug/share"
    "${CURRENT_PACKAGES_DIR}/share/doc"
)
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE.txt")
