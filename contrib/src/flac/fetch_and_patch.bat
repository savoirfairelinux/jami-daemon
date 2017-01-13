set BUILD=%SRC%..\build

set FLAC_VERSION=1.3.0
set FLAC_URL="http://downloads.xiph.org/releases/flac/flac-%FLAC_VERSION%.tar.xz"

mkdir %BUILD%
wget %FLAC_URL%
7z -y e flac-%FLAC_VERSION%.tar.xz  && 7z -y x flac-%FLAC_VERSION%.tar -o%BUILD%
del flac-%FLAC_VERSION%.tar && del flac-%FLAC_VERSION%.tar.xz && rmdir /s /q %BUILD%\PaxHeaders.8635
rename %BUILD%\flac-%FLAC_VERSION% libflac

cd %BUILD%\libflac

rem git apply --reject --whitespace=fix %SRC%\flac\flac-UWP.patch

cd %SRC%