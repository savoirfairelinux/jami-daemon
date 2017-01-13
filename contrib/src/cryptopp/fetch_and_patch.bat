set BUILD=%SRC%..\build

set CRYPTOPP_VERSION=54557b18275053bbfc34594f7e65808dd92dd1a6
set CRYPTOPP_URL="https://github.com/weidai11/cryptopp.git"

mkdir %BUILD%
cd %BUILD%

git clone %CRYPTOPP_URL%
cd cryptopp
git checkout %CRYPTOPP_VERSION%

git apply --reject --whitespace=fix %SRC%\cryptopp\cryptopp-uwp.patch

cd %SRC%