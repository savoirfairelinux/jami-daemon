set BUILD=%SRC%..\build

set ICONV_VERSION=65ab92f7a1699ecc39e37fb81f66e5a42aaa35c4
set ICONV_URL=https://github.com/ShiftMediaProject/libiconv/archive/%ICONV_VERSION%.tar.gz

mkdir %BUILD%
wget %NETTLE_URL%
7z -y x libiconv-%NETTLE_VERSION%.tar.gz && 7z -y x libiconv-%NETTLE_VERSION%.tar -o%BUILD%
del libiconv-%NETTLE_VERSION%.tar && del libiconv-%NETTLE_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\libiconv-%NETTLE_VERSION% libiconv

cd %BUILD%\libiconv

git apply --reject --whitespace=fix %SRC%\iconv\libiconv-uwp.patch

cd %SRC%