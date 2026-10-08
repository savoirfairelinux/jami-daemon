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
not needed: they only bypassed pkg-config, which vcpkg provides. The port also
backports upstream's `-libpath:` → `-L` rewrite of its `.pc` files and declares
`runtimeobject` for the C++/WinRT dxgigrab device.

`ports/webrtc-audio-processing` is the closed upstream PR microsoft/vcpkg#52402
(2.1, meson, abseil) plus `install-vad-header.patch`: APM 2.x has no voice
detection, so the daemon uses the standalone WebRTC VAD whose header upstream
does not install.

`ports/shiftmedia-libgnutls` is copied verbatim from savoirfairelinux/opendht
`ports/` (the future shared registry home). `ports/opendht` follows the contrib
version.

`ports/pjproject` builds the SFL fork's library projects with msbuild; the contrib
Windows patches reduce to `config_site.h` defines.

`ports/natpmp` is Jami's contrib pin and patches (no upstream port).

`ports/dhtnet` patches the MSVC CMake branch to use pkg-config under vcpkg and
guards `unistd.h`; both patches are upstream material.

`ports/portaudio` is Jami's contrib pin (2025, WASAPI only) plus the
`Pa_GetDefaultComm*Device` patch; vcpkg's port is a 2021 snapshot the patch does
not apply to.

`ports/libupnp` is the stock port plus a `.pc` fix for MSVC static (`libupnps`,
`ixmls`, pthreads, `UPNP_STATIC_LIB`).
