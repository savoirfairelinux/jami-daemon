set BUILD=%SRC%..\build
mkdir %BUILD%

set RESTBED_VERSION=bf61912c80572475b83a2fcf0da519f492a4d99e
set RESTBED_URL=https://github.com/Corvusoft/restbed/archive/%RESTBED_VERSION%.tar.gz

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

%APPLY_CMD% %SRC%\restbed\win32_cmake-find-openssl-shared.patch

cd ..

:: submodules

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

if "%1"=="uwp" (
    %APPLY_CMD% %SRC%\restbed\asio-uwp.patch
)
    
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

if "%1"=="uwp" (
    %APPLY_CMD% %SRC%\restbed\openssl-uwp.patch
)

cd %SRC%