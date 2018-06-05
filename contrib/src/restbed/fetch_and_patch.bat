set BUILD=%SRC%..\build

set RESTBED_VERSION=df867a858dddc4cf6ca8642da02720bd65ba239a
set RESTBED_URL=https://github.com/Corvusoft/restbed/archive/%RESTBED_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%RESTBED_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %RESTBED_URL%
)

rem ------------ restbed ------------

7z -y x %RESTBED_VERSION%.tar.gz && 7z -y x %RESTBED_VERSION%.tar -o%BUILD%
del %RESTBED_VERSION%.tar && del %RESTBED_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\restbed-%RESTBED_VERSION% restbed

cd %BUILD%\restbed
%APPLY_CMD% %SRC%\restbed\async_read_until-uwp.patch
%APPLY_CMD% %SRC%\restbed\cmake-uwp.patch

cd ..

rmdir /s /q %BUILD%\restbed\dependency
mkdir %BUILD%\restbed\dependency
cd %BUILD%\restbed\dependency

rem ------------ asio ------------

set ASIO_VERSION=276846097ab5073b67e772dbdfa12596224a54a5
set ASIO_URL=https://github.com/Corvusoft/asio-dependency/archive/%ASIO_VERSION%.tar.gz

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%ASIO_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %ASIO_URL%
)

7z -y x %ASIO_VERSION%.tar.gz && 7z -y x %ASIO_VERSION%.tar
del %ASIO_VERSION%.tar && del %ASIO_VERSION%.tar.gz && del pax_global_header
rename asio-dependency-%ASIO_VERSION% asio

cd asio
%APPLY_CMD% %SRC%\restbed\asio-uwp.patch
cd ..

rem ------------ catch ------------

set CATCH_VERSION=35f510545d55a831372d3113747bf1314ff4f2ef
set CATCH_URL=https://github.com/Corvusoft/catch-dependency/archive/%CATCH_VERSION%.tar.gz

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%CATCH_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %CATCH_URL%
)

7z -y x %CATCH_VERSION%.tar.gz && 7z -y x %CATCH_VERSION%.tar
del %CATCH_VERSION%.tar && del %CATCH_VERSION%.tar.gz && del pax_global_header
rename catch-dependency-%CATCH_VERSION% catch

rem ------------ openssl ------------

set OPENSSL_VERSION=5cc1e25bc76bcf0db03bc37bd64b3290727963b6
set OPENSSL_URL=https://github.com/Microsoft/openssl/archive/%OPENSSL_VERSION%.tar.gz

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%OPENSSL_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %OPENSSL_URL%
)

7z -y x %OPENSSL_VERSION%.tar.gz && 7z -y x %OPENSSL_VERSION%.tar
del %OPENSSL_VERSION%.tar && del %OPENSSL_VERSION%.tar.gz && del pax_global_header
rename openssl-%OPENSSL_VERSION% openssl

cd openssl
%APPLY_CMD% %SRC%\restbed\openssl-uwp.patch
cd ..

rem ------------ kashmir ------------

set KASHMIR_VERSION=2f3913f49c4ac7f9bff9224db5178f6f8f0ff3ee
set KASHMIR_URL=https://github.com/corvusoft/kashmir-dependency/archive/%KASHMIR_VERSION%.tar.gz

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%KASHMIR_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %KASHMIR_URL%
)

7z -y x %KASHMIR_VERSION%.tar.gz && 7z -y x %KASHMIR_VERSION%.tar
del %KASHMIR_VERSION%.tar && del %KASHMIR_VERSION%.tar.gz && del pax_global_header
rename kashmir-dependency-%KASHMIR_VERSION% kashmir

cd %SRC%