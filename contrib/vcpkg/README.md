# vcpkg dependencies (Windows/MSVC)

Proof of concept replacing pywinmake (`compat/msvc/winmake.py`) with vcpkg.

    git clone https://github.com/microsoft/vcpkg %VCPKG_ROOT% && %VCPKG_ROOT%\bootstrap-vcpkg.bat
    cd contrib\vcpkg
    %VCPKG_ROOT%\vcpkg install --triplet x64-windows-static-md-release

`x64-windows-static-md-release` = static libs, dynamic CRT (`/MD`), release only —
what the current MSVC build produces.

`ports/` holds Jami overlays. `ports/ffmpeg` is the upstream vcpkg port at its last
6.1.x revision (microsoft/vcpkg@f00e89ae19) pinned to 6.1.3 with Jami's patches and
component list (`jami-options.cmake`). Jami's `windows-configure*.patch` files are
not needed: they only bypassed pkg-config, which vcpkg provides.

`ports/shiftmedia-libgnutls` is copied verbatim from savoirfairelinux/opendht
`ports/` (the future shared registry home). `ports/opendht` pins opendht master:
the gnutls-fork overlay and OCSP type fixes post-date v4.4.0.

`ports/pjproject` builds the SFL fork's library projects with msbuild; the contrib
Windows patches reduce to `config_site.h` defines. `ports/natpmp` is Jami's contrib
pin and patches (no upstream port). `ports/dhtnet` patches the MSVC CMake branch to
use pkg-config under vcpkg and guards `unistd.h`; both patches are upstream material.
