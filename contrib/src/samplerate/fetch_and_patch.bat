set BUILD=%SRC%..\build

set SAMPLERATE_VERSION=0.1.8
set SAMPLERATE_URL="http://www.mega-nerd.com/SRC/libsamplerate-%SAMPLERATE_VERSION%.tar.gz"

mkdir %BUILD%
wget %SAMPLERATE_URL%
7z -y e libsamplerate-%SAMPLERATE_VERSION%.tar.gz  && 7z -y x libsamplerate-%SAMPLERATE_VERSION%.tar -o%BUILD%
del libsamplerate-%SAMPLERATE_VERSION%.tar && del libsamplerate-%SAMPLERATE_VERSION%.tar.gz
rename %BUILD%\libsamplerate-%SAMPLERATE_VERSION% libsamplerate

cd %BUILD%\libsamplerate

rem git apply --reject --whitespace=fix %SRC%\samplerate\samplerate-UWP.patch

cd %SRC%