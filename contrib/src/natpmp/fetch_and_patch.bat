set BUILD=%SRC%..\build

set NATPMP_VERSION=6d5c0db6a06036fcd97bdba10b474d5369582dba
set NATPMP_URL=http://github.com/miniupnp/libnatpmp/archive/%NATPMP_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%NATPMP_VERSION%.zip %cd%
) else (
    %WGET_CMD% %NATPMP_URL%
)

7z -y x %NATPMP_VERSION%.tar.gz && 7z -y x %NATPMP_VERSION%.tar -o%BUILD%
del %NATPMP_VERSION%.tar && del %NATPMP_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\libnatpmp-%NATPMP_VERSION% natpmp

cd %BUILD%\natpmp

mkdir msvc && cd msvc
setlocal
set PATH=C:\\Program Files\\CMake\\bin\\;%PATH%
cmake .. -G "Visual Studio 15 2017 Win64"
endlocal

cd %SRC%