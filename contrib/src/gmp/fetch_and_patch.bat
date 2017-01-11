set BUILD=%SRC%..\build

set REPOURL="https://github.com/ShiftMediaProject/gmp.git"

mkdir %BUILD%
cd %BUILD%

git clone %REPOURL%

cd gmp
git apply --reject --whitespace=fix %SRC%\gmp\gmp-uwp.patch

cd %SRC%