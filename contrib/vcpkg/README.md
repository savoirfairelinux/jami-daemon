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
