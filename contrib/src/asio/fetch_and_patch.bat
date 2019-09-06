set BUILD=%SRC%..\build

set ASIO_VERSION=asio-1-12-2
set ASIO_URL=https://github.com/chriskohlhoff/asio/archive/%ASIO_VERSION%.tar.gz

mkdir %BUILD%

%WGET_CMD% %ASIO_URL%

7z -y x %ASIO_VERSION%.tar.gz && 7z -y x %ASIO_VERSION%.tar -o%BUILD%
del %ASIO_VERSION%.tar && del %ASIO_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\asio-%ASIO_VERSION% asio

cd %BUILD%\asio

%APPLY_CMD% %SRC%asio\asio-vcxproj.patch

if "%1"=="uwp" (
    %APPLY_CMD% %SRC%asio\asio-uwp.patch
)

cd %SRC%