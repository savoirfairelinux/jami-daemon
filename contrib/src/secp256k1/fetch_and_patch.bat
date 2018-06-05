set BUILD=%SRC%..\build

set SECP256K1_VERSION=0b7024185045a49a1a6a4c5615bf31c94f63d9c4
set SECP256K1_URL="https://github.com/bitcoin-core/secp256k1/archive/%SECP256K1_VERSION%.tar.gz"

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\libsamplerate-%SECP256K1_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %SECP256K1_URL%
)

7z -y e %SECP256K1_VERSION%.tar.gz  && 7z -y x %SECP256K1_VERSION%.tar -o%BUILD%
del %SECP256K1_VERSION%.tar && del %SECP256K1_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\secp256k1-%SECP256K1_VERSION% secp256k1

cd %BUILD%\secp256k1

%APPLY_CMD% %SRC%\secp256k1\secp256k1-vs2017.patch

cd %SRC%