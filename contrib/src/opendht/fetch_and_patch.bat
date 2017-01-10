set BUILD=..\..\build
set SRC=%~dp0%~1

set OPENDHT_VERSION=e7295bac7b57540905e287a37904c615de971392
set OPENDHT_URL="https://github.com/savoirfairelinux/opendht.git"

mkdir %BUILD%
cd %BUILD%

git clone %OPENDHT_URL%
cd opendht
git checkout %OPENDHT_VERSION%

cd %SRC%