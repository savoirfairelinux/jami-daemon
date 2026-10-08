# Include before project(), which runs the vcpkg manifest install: with vcpkg's
# toolchain on Windows, dependencies come from this manifest instead of pywinmake.
# Used by the daemon and by projects embedding it (e.g. the Qt client).
if(CMAKE_HOST_WIN32 AND CMAKE_TOOLCHAIN_FILE MATCHES "vcpkg\\.cmake$")
    set(VCPKG_MANIFEST_DIR "${CMAKE_CURRENT_LIST_DIR}" CACHE PATH "")
    # Default to the triplet the overlay ports are built and tested with;
    # -DVCPKG_TARGET_TRIPLET or VCPKG_DEFAULT_TRIPLET in the environment win.
    if(NOT DEFINED VCPKG_TARGET_TRIPLET AND NOT DEFINED ENV{VCPKG_DEFAULT_TRIPLET})
        set(VCPKG_TARGET_TRIPLET "x64-windows-static-md-release" CACHE STRING "")
        set(VCPKG_HOST_TRIPLET "${VCPKG_TARGET_TRIPLET}" CACHE STRING "")
    endif()
endif()
