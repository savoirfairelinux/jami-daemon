set BUILD=%SRC%..\build

set REPOURL="https://github.com/open-source-parsers/jsoncpp.git"

mkdir %BUILD%
cd %BUILD%

git clone %REPOURL%
cd jsoncpp

git apply --reject --whitespace=fix %SRC%\jsoncpp\jsoncpp-uwp.patch

cd %SRC%