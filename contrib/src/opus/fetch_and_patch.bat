set BUILD=%SRC%..\build

set OPUS_VERSION=db56e664f432144a022d74a92e4ede1a5be19497
set OPUS_URL=https://github.com/ShiftMediaProject/opus/archive/%OPUS_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%OPUS_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %OPUS_URL%
)

7z -y x %OPUS_VERSION%.tar.gz && 7z -y x %OPUS_VERSION%.tar -o%BUILD%
del %OPUS_VERSION%.tar && del %OPUS_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\opus-%OPUS_VERSION% opus

cd %SRC%