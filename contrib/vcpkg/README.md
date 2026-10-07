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

`ports/` holds Jami overlays. `ports/ffmpeg` is the upstream vcpkg port at its last
6.1.x revision (microsoft/vcpkg@f00e89ae19) pinned to 6.1.3 with Jami's patches and
component list (`jami-options.cmake`). Jami's `windows-configure*.patch` files are
not needed: they only bypassed pkg-config, which vcpkg provides.

`ports/shiftmedia-libgnutls` is copied verbatim from savoirfairelinux/opendht
`ports/` (the future shared registry home). `ports/opendht` follows the contrib
version.

`ports/pjproject` builds the SFL fork's library projects with msbuild; the contrib
Windows patches reduce to `config_site.h` defines. `ports/natpmp` is Jami's contrib
pin and patches (no upstream port). `ports/dhtnet` patches the MSVC CMake branch to
use pkg-config under vcpkg and guards `unistd.h`; both patches are upstream material.

`ports/portaudio` is Jami's contrib pin (2025, WASAPI only) plus the
`Pa_GetDefaultComm*Device` patch; vcpkg's port is a 2021 snapshot the patch does
not apply to. `ports/webrtc-audio-processing` is the closed upstream PR
microsoft/vcpkg#52402 (2.1, meson, abseil) plus `install-vad-header.patch`: APM 2.x
has no voice detection, so the daemon uses the standalone WebRTC VAD whose header
upstream does not install.

`ports/yffi` (Windows only in the manifest; other platforms keep `contrib/src/yffi`)
runs cargo on Jami's y-crdt pin and installs `yrs.lib`, `libyrs.h` and a `yrs.pc`
matching the Unix contrib. Rust is a host requirement vcpkg cannot fetch; since
vcpkg scrubs the environment for port builds, `CARGO_HOME`/`RUSTUP_HOME` are read
from the registry when not passed through with `VCPKG_KEEP_ENV_VARS`.

`ports/libupnp` is the stock port plus a `.pc` fix for MSVC static (`libupnps`,
`ixmls`, pthreads, `UPNP_STATIC_LIB`). `ports/ffmpeg` also backports upstream's
`-libpath:` → `-L` rewrite of its `.pc` files and declares `runtimeobject` for the
C++/WinRT dxgigrab device.

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
