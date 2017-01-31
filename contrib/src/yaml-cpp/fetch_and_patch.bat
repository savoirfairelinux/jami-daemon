set BUILD=%SRC%..\build

set YAMLCPP_VERSION=24fa1b33805c9a91df0f32c46c28e314dd7ad96f
set YAMLCPP_URL=https://github.com/jbeder/yaml-cpp/archive/%YAMLCPP_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%YAMLCPP_VERSION%.tar.gz %cd%
) else (
    wget %YAMLCPP_URL%
)

7z -y x %YAMLCPP_VERSION%.tar.gz && 7z -y x %YAMLCPP_VERSION%.tar -o%BUILD%
del %YAMLCPP_VERSION%.tar && del %YAMLCPP_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\yaml-cpp-%YAMLCPP_VERSION% yaml-cpp

cd %BUILD%\yaml-cpp

git apply --reject --whitespace=fix %SRC%\yaml-cpp\yaml-cpp-uwp.patch

cd %SRC%