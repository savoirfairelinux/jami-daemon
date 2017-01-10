set BUILD=%SRC%..\build

set REPOURL="https://github.com/ShiftMediaProject/libiconv.git"

mkdir %BUILD%
cd %BUILD%

git clone %REPOURL%

cd %SRC%