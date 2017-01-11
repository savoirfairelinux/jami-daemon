set BUILD=%SRC%..\build

set OPENDHT_VERSION=3fb7c2acfe098b18908d1803e5aaa2437d878af4
set OPENDHT_URL="https://github.com/savoirfairelinux/opendht.git"

mkdir %BUILD%
cd %BUILD%

git clone %OPENDHT_URL%
cd opendht
git checkout %OPENDHT_VERSION%

cd %SRC%