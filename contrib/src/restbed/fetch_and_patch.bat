set BUILD=%SRC%..\build

set RESTBED_VERSION=34187502642144ab9f749ab40f5cdbd8cb17a54a
set RESTBED_URL="https://github.com/Corvusoft/restbed.git"

mkdir %BUILD%
cd %BUILD%

git clone --recursive %RESTBED_URL%
cd restbed
git submodule foreach git pull origin master
git checkout %RESTBED_VERSION%

rem git apply --reject --whitespace=fix %SRC%\restbed\restbed-UWP.patch

cd %SRC%