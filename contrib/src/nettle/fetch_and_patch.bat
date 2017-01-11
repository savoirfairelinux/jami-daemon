set BUILD=%SRC%..\build

set REPOURL="https://github.com/ShiftMediaProject/nettle.git"

mkdir %BUILD%
cd %BUILD%

git clone %REPOURL%

cd nettle
git apply --reject --whitespace=fix %SRC%\nettle\nettle-uwp.patch

cd %SRC%