set BUILD=%SRC%..\build

set VORBIS_VERSION=1.3.4
set VORBIS_URL="http://downloads.xiph.org/releases/vorbis/libvorbis-%VORBIS_VERSION%.tar.xz"

mkdir %BUILD%
wget %VORBIS_URL%
7z -y e libvorbis-%VORBIS_VERSION%.tar.xz  && 7z -y x libvorbis-%VORBIS_VERSION%.tar -o%BUILD%
del libvorbis-%VORBIS_VERSION%.tar && del libvorbis-%VORBIS_VERSION%.tar.xz
rename %BUILD%\libvorbis-%VORBIS_VERSION% vorbis

cd %BUILD%\vorbis

rem git apply --reject --whitespace=fix %SRC%\vorbis\vorbis-UWP.patch

cd %SRC%