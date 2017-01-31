set BUILD=%SRC%..\build

set BOOST_VERSION_1=1.61.0
set BOOST_VERSION_2=1_61_0
set BOOST_URL="https://sourceforge.net/projects/boost/files/boost/%BOOST_VERSION_1%/boost_%BOOST_VERSION_2%.zip"

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\boost_%BOOST_VERSION_2%.zip %cd%
) else (
    wget %BOOST_URL%
)

unzip -q boost_%BOOST_VERSION_2%.zip -d %BUILD%
del boost_%BOOST_VERSION_2%.zip
rename %BUILD%\boost_%BOOST_VERSION_2% boost

cd %BUILD%\boost

git apply --reject --whitespace=fix %SRC%\boost\boost-uwp.patch

cd %SRC%