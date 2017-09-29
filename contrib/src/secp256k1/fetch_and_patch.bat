set BUILD=%SRC%..\build

set SECP256K1_VERSION=0b7024185045a49a1a6a4c5615bf31c94f63d9c4
set SECP256K1_URL="https://github.com/bitcoin-core/secp256k1/archive/%SECP256K1_VERSION%.tar.gz"

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\libsamplerate-%SECP256K1_VERSION%.tar.gz %cd%
) else (
    wget %SECP256K1_URL%
)

7z -y e %SECP256K1_VERSION%.tar.gz  && 7z -y x %SECP256K1_VERSION%.tar -o%BUILD%
del %SECP256K1_VERSION%.tar && del %SECP256K1_VERSION%.tar.gz
rename %BUILD%\secp256k1-%SECP256K1_VERSION% secp256k1

cd %BUILD%\secp256k1

git apply --reject --whitespace=fix %SRC%\secp256k1\secp256k1-uwp.patch

cd %SRC%