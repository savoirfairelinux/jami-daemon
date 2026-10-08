vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO y-crdt/y-crdt
    REF 03e14a0232903498299a9e717c7ee8001e40e5db
    SHA512 17b08644cdedcfb37bc891ab4eb657e2c8ed1d424c0e41f50825f4d527c7eb7bb39e402a64a7ec2b8df12250a1c8faa3122391c47b514f8773df004e3c15816c
    HEAD_REF main
    PATCHES
        0001-avoid-if-let-guard.patch
)

# vcpkg cannot fetch a Rust toolchain; cargo is a host requirement like a compiler.
# vcpkg scrubs the environment for port builds, so rustup's CARGO_HOME/RUSTUP_HOME
# are read back from the registry (or passed through with VCPKG_KEEP_ENV_VARS).
foreach(var CARGO_HOME RUSTUP_HOME)
    if(NOT DEFINED ENV{${var}})
        foreach(key "HKEY_CURRENT_USER\\Environment" "HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment")
            get_filename_component(value "[${key};${var}]" ABSOLUTE)
            if(IS_DIRECTORY "${value}")
                set(ENV{${var}} "${value}")
                break()
            endif()
        endforeach()
    endif()
endforeach()
find_program(CARGO cargo PATHS "$ENV{CARGO_HOME}/bin" "$ENV{USERPROFILE}/.cargo/bin" "$ENV{HOME}/.cargo/bin")
if(NOT CARGO)
    message(FATAL_ERROR "yffi: cargo not found. Install the Rust toolchain (https://rustup.rs) or add it to PATH.")
endif()

if(VCPKG_TARGET_IS_WINDOWS)
    if(VCPKG_TARGET_ARCHITECTURE STREQUAL "x64")
        set(RUST_TARGET x86_64-pc-windows-msvc)
    elseif(VCPKG_TARGET_ARCHITECTURE STREQUAL "arm64")
        set(RUST_TARGET aarch64-pc-windows-msvc)
    else()
        message(FATAL_ERROR "yffi: unsupported architecture ${VCPKG_TARGET_ARCHITECTURE}")
    endif()
    # rustc --print native-static-libs, minus the windows-sys import lib whose
    # symbols the system libs below already provide.
    set(PRIVATE_LIBS "-lkernel32 -lntdll -luserenv -lws2_32 -ldbghelp -lbcrypt -ladvapi32")
else()
    message(FATAL_ERROR "yffi: this overlay only targets Windows; other platforms use contrib/src/yffi")
endif()

# Rust defaults to the dynamic CRT (/MD) on MSVC; follow the triplet.
if(VCPKG_CRT_LINKAGE STREQUAL "static")
    set(ENV{RUSTFLAGS} "$ENV{RUSTFLAGS} -C target-feature=+crt-static")
endif()

set(TARGET_DIR "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}/target")

function(yffi_build PROFILE_FLAG PROFILE_DIR OUT_LIBDIR)
    vcpkg_execute_required_process(
        COMMAND "${CARGO}" build ${PROFILE_FLAG} --locked
                --manifest-path "${SOURCE_PATH}/yffi/Cargo.toml"
                --target-dir "${TARGET_DIR}" --target ${RUST_TARGET}
        WORKING_DIRECTORY "${SOURCE_PATH}"
        LOGNAME "build-${TARGET_TRIPLET}-${PROFILE_DIR}"
    )
    file(INSTALL "${TARGET_DIR}/${RUST_TARGET}/${PROFILE_DIR}/yrs.lib" DESTINATION "${OUT_LIBDIR}")
endfunction()

yffi_build(--release release "${CURRENT_PACKAGES_DIR}/lib")
if(NOT VCPKG_BUILD_TYPE)
    yffi_build("" debug "${CURRENT_PACKAGES_DIR}/debug/lib")
endif()

file(INSTALL "${SOURCE_PATH}/tests-ffi/include/libyrs.h" DESTINATION "${CURRENT_PACKAGES_DIR}/include")

# Same yrs.pc the Unix contrib generates (contrib/src/yffi/yrs.pc.in).
file(WRITE "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/yrs.pc" "prefix=${CURRENT_PACKAGES_DIR}
exec_prefix=\${prefix}
libdir=\${prefix}/lib
includedir=\${prefix}/include

Name: yrs
Description: Y-CRDT (yrs) C FFI - CRDT engine for collaborative editing
Version: ${VERSION}
Libs: -L\${libdir} -lyrs
Libs.private: ${PRIVATE_LIBS}
Cflags: -I\${includedir}
")
if(NOT VCPKG_BUILD_TYPE)
    file(COPY "${CURRENT_PACKAGES_DIR}/lib/pkgconfig/yrs.pc" DESTINATION "${CURRENT_PACKAGES_DIR}/debug/lib/pkgconfig")
endif()
vcpkg_fixup_pkgconfig()

vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
