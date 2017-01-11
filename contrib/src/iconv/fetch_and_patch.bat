set BUILD=%SRC%..\build

set REPOURL="https://github.com/ShiftMediaProject/libiconv.git"

mkdir %BUILD%
cd %BUILD%

git clone %REPOURL%

cd libiconv
git apply --reject --whitespace=fix %SRC%\iconv\libiconv-uwp.patch

cd %SRC%