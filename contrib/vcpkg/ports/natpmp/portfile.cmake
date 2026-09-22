vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO miniupnp/libnatpmp
    REF 0c56980a3dcfab08bd7dd145d49fb4868fbaf1ca
    SHA512 334f0c9fa7738ecb7a5ffe63477b7a2290ea4ddb64f4d76dbd80fcf47e07960a69b3ef6b147dad79d7e8886ca469987ac097a1ef5d9bf0007b2811b17f6b6de1
    HEAD_REF master
    PATCHES
        0001-Add-NATPMP_BUILD_TOOLS-option-to-conditionally-build.patch
        natpmp-win32-ssize_t.patch
)

# Jami setting from contrib/src/natpmp/package.json (CMAKE_C_FLAGS there);
# passing CMAKE_C_FLAGS through vcpkg would clobber the toolchain flags.
vcpkg_replace_string("${SOURCE_PATH}/CMakeLists.txt"
    "-DENABLE_STRNATPMPERR" "-DENABLE_STRNATPMPERR -DNATPMP_MAX_RETRIES=3")

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS -DNATPMP_BUILD_TOOLS=OFF
)
vcpkg_cmake_install()
vcpkg_fixup_pkgconfig()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
