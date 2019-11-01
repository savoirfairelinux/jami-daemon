set BUILD=%SRC%..\build

set RESTINIO_VERSION=8d5d3e8237e0947adb9ba1ffc8281f4ad7cb2a59
set RESTINIO_URL=https://github.com/Stiffstream/restinio/archive/%RESTINIO_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%RESTINIO_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %RESTINIO_URL%
)

7z -y x %RESTINIO_VERSION%.tar.gz && 7z -y x %RESTINIO_VERSION%.tar -o%BUILD%
del %RESTINIO_VERSION%.tar && del %RESTINIO_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\restinio-%RESTINIO_VERSION% restinio

cd %BUILD%\restinio

cd %SRC%
