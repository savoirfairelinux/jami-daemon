set BUILD=%SRC%..\build

set JSONCPP_VERSION=81065748e315026017c633fca1bfc57cba5b246a
set JSONCPP_URL=https://github.com/open-source-parsers/jsoncpp/archive/%JSONCPP_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%JSONCPP_VERSION%.tar.gz %cd%
) else (
    wget %JSONCPP_URL%
)

7z -y x %JSONCPP_VERSION%.tar.gz && 7z -y x %JSONCPP_VERSION%.tar -o%BUILD%
del %JSONCPP_VERSION%.tar && del %JSONCPP_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\jsoncpp-%JSONCPP_VERSION% jsoncpp

cd %BUILD%\jsoncpp

git apply --reject --whitespace=fix %SRC%\jsoncpp\jsoncpp-uwp.patch

cd %SRC%