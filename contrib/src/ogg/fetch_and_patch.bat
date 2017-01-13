set BUILD=%SRC%..\build

set OGG_VERSION=1.3.1
set OGG_URL="http://downloads.xiph.org/releases/ogg/libogg-%OGG_VERSION%.tar.xz"

mkdir %BUILD%
wget %OGG_URL%
7z -y e libogg-%OGG_VERSION%.tar.xz  && 7z -y x libogg-%OGG_VERSION%.tar -o%BUILD%
del libogg-%OGG_VERSION%.tar && del libogg-%OGG_VERSION%.tar.xz
rename %BUILD%\libogg-%OGG_VERSION% ogg

cd %BUILD%\ogg

rem git apply --reject --whitespace=fix %SRC%\ogg\ogg-UWP.patch

cd %SRC%