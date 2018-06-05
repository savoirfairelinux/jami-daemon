set BUILD=%SRC%..\build

set GMP_VERSION=eb35fdadc072ecae2b262fd6e6709c308cadc07a
set GMP_URL=https://github.com/ShiftMediaProject/gmp/archive/%GMP_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%GMP_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %GMP_URL%
)

7z -y x %GMP_VERSION%.tar.gz && 7z -y x %GMP_VERSION%.tar -o%BUILD%
del %GMP_VERSION%.tar && del %GMP_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\gmp-%GMP_VERSION% gmp

cd %SRC%