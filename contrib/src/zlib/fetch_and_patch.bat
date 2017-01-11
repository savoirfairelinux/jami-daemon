set BUILD=%SRC%..\build

set REPOURL="https://github.com/ShiftMediaProject/zlib.git"

mkdir %BUILD%
cd %BUILD%

git clone %REPOURL%

cd zlib
git apply --reject --whitespace=fix %SRC%\zlib\zlib-uwp.patch

cd %SRC%