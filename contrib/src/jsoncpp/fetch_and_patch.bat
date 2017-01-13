set BUILD=%SRC%..\build

set REPOURL="https://github.com/open-source-parsers/jsoncpp.git"

mkdir %BUILD%
cd %BUILD%

git clone %REPOURL%

cd %SRC%