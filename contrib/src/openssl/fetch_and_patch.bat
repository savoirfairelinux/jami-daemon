set BUILD=%SRC%..\build

set OPENSSL_VERSION=5cc1e25bc76bcf0db03bc37bd64b3290727963b6
set OPENSSL_URL=https://github.com/Microsoft/openssl/archive/%OPENSSL_VERSION%.tar.gz

mkdir %BUILD%

%WGET_CMD% %OPENSSL_URL%

7z -y x %OPENSSL_VERSION%.tar.gz && 7z -y x %OPENSSL_VERSION%.tar
del %OPENSSL_VERSION%.tar && del %OPENSSL_VERSION%.tar.gz && del pax_global_header
rename openssl-%OPENSSL_VERSION% openssl

cd openssl

if "%1"=="uwp" (
    %APPLY_CMD% %SRC%\openssl-uwp.patch
)

cd %SRC%