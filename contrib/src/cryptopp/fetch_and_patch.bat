set BUILD=%SRC%..\build

set CRYPTOPP_VERSION=54557b18275053bbfc34594f7e65808dd92dd1a6
set CRYPTOPP_URL=https://github.com/weidai11/cryptopp/archive/%CRYPTOPP_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%CRYPTOPP_VERSION%.tar.gz %cd%
) else (
    wget %CRYPTOPP_URL%
)

7z -y x %CRYPTOPP_VERSION%.tar.gz && 7z -y x %CRYPTOPP_VERSION%.tar -o%BUILD%
del %CRYPTOPP_VERSION%.tar && del %CRYPTOPP_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\cryptopp-%CRYPTOPP_VERSION% cryptopp

cd %BUILD%\cryptopp

git apply --reject --whitespace=fix %SRC%\cryptopp\cryptopp-uwp.patch

cd %SRC%