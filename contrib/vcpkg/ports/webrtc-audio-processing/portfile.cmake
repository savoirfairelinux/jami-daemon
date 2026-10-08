vcpkg_from_gitlab(
    GITLAB_URL "https://gitlab.freedesktop.org/"
    OUT_SOURCE_PATH SOURCE_PATH
    REPO "pulseaudio/webrtc-audio-processing"
    # Same pin as contrib/src/webrtc-audio-processing: v2.1 plus the
    # abseil 202508 and gcc-15 build fixes.
    REF d0569cfa50c1858ee279d77b3fc8870be6902441
    SHA512 7e5b76ed636fcdf8b0fd109d899b8d2f87175623f14e2982433dc05d42bf09af405c1db49ff8f57ab0eacb9ec5cd61a5d33fbc69655c3fc3c06f668bf4107981
    PATCHES
        # WebRTC typically builds with clang, but VCPKG typically builds with
        # MSVC on Windows. Avoid some clang-specific assembly code in that
        # scenario, with the same fallback used for x86.
        fix-asm-windows-arm.patch

        # Jami: APM 2.x has no voice detection; expose the standalone WebRTC VAD.
        install-vad-header.patch
)

set(MESON_OPTIONS "")
if(VCPKG_TARGET_IS_WINDOWS)
    # Designated initializers in the WebRTC source require C++20 on MSVC
    list(APPEND MESON_OPTIONS "-Dcpp_std=vc++20")
endif()

vcpkg_configure_meson(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS ${MESON_OPTIONS}
)

vcpkg_install_meson()
vcpkg_fixup_pkgconfig()
vcpkg_copy_pdbs()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")

# Remove bin dirs for static builds or non-Windows (no DLL)
if(VCPKG_LIBRARY_LINKAGE STREQUAL "static" OR NOT VCPKG_TARGET_IS_WINDOWS)
    file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/bin" "${CURRENT_PACKAGES_DIR}/debug/bin")
endif()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/COPYING")
file(INSTALL "${CMAKE_CURRENT_LIST_DIR}/usage" DESTINATION "${CURRENT_PACKAGES_DIR}/share/${PORT}")