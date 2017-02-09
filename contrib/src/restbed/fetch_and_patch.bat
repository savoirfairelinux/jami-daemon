set BUILD=%SRC%..\build

set RESTBED_VERSION=34187502642144ab9f749ab40f5cdbd8cb17a54a
set RESTBED_URL=https://github.com/Corvusoft/restbed/archive/%RESTBED_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%RESTBED_VERSION%.tar.gz %cd%
) else (
    wget %RESTBED_URL%
)

7z -y x %RESTBED_VERSION%.tar.gz && 7z -y x %RESTBED_VERSION%.tar -o%BUILD%
del %RESTBED_VERSION%.tar && del %RESTBED_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\restbed-%RESTBED_VERSION% restbed

rmdir /s /q %BUILD%\restbed\dependency
mkdir %BUILD%\restbed\dependency
cd %BUILD%\restbed\dependency

set ASIO_VERSION=276846097ab5073b67e772dbdfa12596224a54a5
set ASIO_URL=https://github.com/Corvusoft/asio-dependency/archive/%ASIO_VERSION%.tar.gz

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%ASIO_VERSION%.tar.gz %cd%
) else (
    wget %ASIO_URL%
)

7z -y x %ASIO_VERSION%.tar.gz && 7z -y x %ASIO_VERSION%.tar
del %ASIO_VERSION%.tar && del %ASIO_VERSION%.tar.gz && del pax_global_header
rename asio-dependency-%ASIO_VERSION% asio

cd asio
git apply --reject --whitespace=fix %SRC%\restbed\asio-uwp.patch

cd ..

set CATCH_VERSION=35f510545d55a831372d3113747bf1314ff4f2ef
set CATCH_URL=https://github.com/Corvusoft/catch-dependency/archive/%CATCH_VERSION%.tar.gz

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%CATCH_VERSION%.tar.gz %cd%
) else (
    wget %CATCH_URL%
)

7z -y x %CATCH_VERSION%.tar.gz && 7z -y x %CATCH_VERSION%.tar
del %CATCH_VERSION%.tar && del %CATCH_VERSION%.tar.gz && del pax_global_header
rename catch-dependency-%CATCH_VERSION% catch

set OPENSSL_VERSION=c7ba244789ce9f9b6675ff88e61dd5d5e5cac53e
set OPENSSL_URL=https://github.com/Microsoft/openssl/archive/%OPENSSL_VERSION%.tar.gz

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%OPENSSL_VERSION%.tar.gz %cd%
) else (
    wget %OPENSSL_URL%
)

7z -y x %OPENSSL_VERSION%.tar.gz && 7z -y x %OPENSSL_VERSION%.tar
del %OPENSSL_VERSION%.tar && del %OPENSSL_VERSION%.tar.gz && del pax_global_header
rename openssl-%OPENSSL_VERSION% openssl

cd %SRC%