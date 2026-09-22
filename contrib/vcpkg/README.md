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
