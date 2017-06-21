set BUILD=%SRC%..\build

set OPUS_VERSION=4e59fb88fac0a4bfa616a7b1c2ec752baa610ef6
set OPUS_URL=https://github.com/ShiftMediaProject/opus/archive/%OPUS_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%OPUS_VERSION%.tar.gz %cd%
) else (
    wget %OPUS_URL%
)

7z -y x %OPUS_VERSION%.tar.gz && 7z -y x %OPUS_VERSION%.tar -o%BUILD%
del %OPUS_VERSION%.tar && del %OPUS_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\opus-%OPUS_VERSION% opus

cd %BUILD%\opus

git apply --reject --whitespace=fix %SRC%\opus\opus-uwp.patch

cd %SRC%