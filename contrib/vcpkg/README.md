# vcpkg dependencies (Windows/MSVC)

Windows dependencies of the daemon, built with vcpkg in manifest mode. Use a
vcpkg clone checked out at the baseline in `vcpkg-configuration.json`:

    git clone https://github.com/microsoft/vcpkg %VCPKG_ROOT%
    git -C %VCPKG_ROOT% checkout <baseline>
    %VCPKG_ROOT%\bootstrap-vcpkg.bat -disableMetrics
    cd contrib\vcpkg
    %VCPKG_ROOT%\vcpkg install --triplet x64-windows-static-md-release

`x64-windows-static-md-release` = static libs, dynamic CRT (`/MD`), release only —
what the current MSVC build produces.

`ports/` holds Jami overlays. `ports/ffmpeg` follows the contrib FFmpeg 9.0.2 pin,
with the upstream vcpkg compatibility patches and Jami's patches and component
list (`jami-options.cmake`). Intel QSV uses libvpl. The port preserves
upstream's `-libpath:` → `-L` rewrite of its `.pc` files and declares
`runtimeobject` for the C++/WinRT dxgigrab device.
The DXGI device publishes resized-window dimensions with each valid packet,
allowing the daemon to restart decoding without calling private FFmpeg callbacks.

`ports/webrtc-audio-processing` is the closed upstream PR microsoft/vcpkg#52402
(2.1, meson, abseil) plus `install-vad-header.patch`: APM 2.x has no voice
detection, so the daemon uses the standalone WebRTC VAD whose header upstream
does not install.

`ports/shiftmedia-libgnutls` is copied verbatim from savoirfairelinux/opendht
`ports/` (the future shared registry home). `ports/opendht` follows the contrib
version.

`ports/pjproject` builds the SFL fork's library projects with msbuild; Jami's
Windows settings are `config_site.h` defines.

`ports/natpmp` is Jami's contrib pin and patches (no upstream port).

`ports/dhtnet` patches the MSVC CMake branch to use pkg-config under vcpkg and
guards `unistd.h`; both patches are upstream material.

`ports/portaudio` is Jami's contrib pin (2025, WASAPI only) plus the
`Pa_GetDefaultComm*Device` patch; vcpkg's port is a 2021 snapshot the patch does
not apply to.

`ports/libupnp` is the stock port plus a `.pc` fix for MSVC static (`libupnps`,
`ixmls`, pthreads, `UPNP_STATIC_LIB`).

`ports/yffi` (Windows only in the manifest; other platforms keep `contrib/src/yffi`)
runs cargo on Jami's y-crdt pin and installs `yrs.lib`, `libyrs.h` and a `yrs.pc`
matching the Unix contrib. Rust is a host requirement vcpkg cannot fetch; since
vcpkg scrubs the environment for port builds, `CARGO_HOME`/`RUSTUP_HOME` are read
from the registry when not passed through with `VCPKG_KEEP_ENV_VARS`.

## Updating dependencies

From `contrib/`, use Python 3 and curl to update both Make metadata and the
matching vcpkg overlay:

```sh
./src/update_contrib dhtnet 01333a9d
./src/update_contrib opendht 4.4.1 --dry-run
```

The updater reads the archive name and URL from `src/<package>/rules.mak`,
updates its version and matching `SHA512SUMS` entry, and updates legacy
`package.json` metadata only when present. Missing checksum files are created;
other checksum entries are preserved.
For vcpkg, it updates the primary `vcpkg_from_github`, `vcpkg_from_gitlab`, or
`vcpkg_download_distfile` source and its SHA-512 checksum. Auxiliary downloads
and patches are left alone. Each distinct URL is downloaded and hashed
separately, since Make and vcpkg may use different archives for the same version.

Release updates change the manifest version and reset `port-version`. Commit
updates preserve numeric/date manifest versions and increment `port-version`
when the source changes. The `upnp`/`libupnp` and
`gnutls`/`shiftmedia-libgnutls` aliases are recognized in both directions;
`--port <name>` selects another overlay. Packages with only Make rules or only
a vcpkg overlay are supported too.

When vcpkg pins a fork commit rather than a release tag, supply its new ref
explicitly, for example:

```sh
./src/update_contrib gnutls <release-version> --vcpkg-ref <fork-commit>
```

All downloads and metadata validation finish before files are replaced. Failed
replacements are rolled back. `--dry-run` still downloads and validates the
archives but does not write metadata. Unsupported or ambiguous Make/CMake
expressions are rejected instead of evaluated. Review the resulting diff and
verify existing patches against the new sources before rebuilding.

Run the offline updater tests with:

```sh
python3 src/test_update_contrib.py
```

## Building the daemon

The daemon CMake uses this manifest automatically when configured with the vcpkg
toolchain (`settings.cmake` sets `VCPKG_MANIFEST_DIR` and the triplet) and
resolves dependencies through pkg-config like other platforms:

    cmake -B build -G "Visual Studio 17 2022" -A x64 ^
        -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
    cmake --build build --config Release --target jami-core

`jami-core.lib` is no longer a fat library: consumers get dependencies through
its `PkgConfig::` link interface. A project using the daemon through
`add_subdirectory` must include `contrib/vcpkg/settings.cmake` before its
own `project()`.
