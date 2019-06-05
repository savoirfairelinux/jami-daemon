set BUILD=%SRC%..\build

set MSGPACK_VERSION=cpp-3.2.0
set MSGPACK_URL=https://github.com/msgpack/msgpack-c/archive/%MSGPACK_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%MSGPACK_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %MSGPACK_URL%
)

7z -y x %MSGPACK_VERSION%.tar.gz && 7z -y x %MSGPACK_VERSION%.tar -o%BUILD%
del %MSGPACK_VERSION%.tar && del %MSGPACK_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\msgpack-c-%MSGPACK_VERSION% msgpack-c

cd %BUILD%\msgpack-c

mkdir vs2017
cd vs2017
cmake .. -DMSGPACK_CXX11=ON -G "Visual Studio 15 2017 Win64"

cd %SRC%