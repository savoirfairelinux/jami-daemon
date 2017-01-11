set BUILD=%SRC%..\build

set MSGPACK_VERSION=1df97bc37b363a340c5ad06c5cbcc53310aaff80
set MSGPACK_URL="https://github.com/msgpack/msgpack-c.git"

mkdir %BUILD%
cd %BUILD%

git clone %MSGPACK_URL%
cd msgpack-c
git checkout %MSGPACK_VERSION%

git apply --reject --whitespace=fix %SRC%\msgpack\msgpack-uwp.patch

cd %SRC%