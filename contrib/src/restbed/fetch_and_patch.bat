set BUILD=%SRC%..\build

set RESTBED_VERSION=df867a858dddc4cf6ca8642da02720bd65ba239a
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

cd %BUILD%\restbed
git apply --reject --whitespace=fix %SRC%\restbed\async_read_until-uwp.patch
git apply --reject --whitespace=fix %SRC%\restbed\cmake-uwp.patch

cd ..

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

set KASHMIR_VERSION=2f3913f49c4ac7f9bff9224db5178f6f8f0ff3ee
set KASHMIR_URL=https://github.com/corvusoft/kashmir-dependency/archive/%KASHMIR_VERSION%.tar.gz

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%KASHMIR_VERSION%.tar.gz %cd%
) else (
    wget %KASHMIR_URL%
)

7z -y x %KASHMIR_VERSION%.tar.gz && 7z -y x %KASHMIR_VERSION%.tar
del %KASHMIR_VERSION%.tar && del %KASHMIR_VERSION%.tar.gz && del pax_global_header
rename kashmir-dependency-%KASHMIR_VERSION% kashmir

cd %SRC%