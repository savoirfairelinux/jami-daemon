set BUILD=%SRC%..\build

set RESTBED_VERSION=34187502642144ab9f749ab40f5cdbd8cb17a54a
set RESTBED_URL="https://github.com/Corvusoft/restbed.git"

mkdir %BUILD%
cd %BUILD%

git clone %RESTBED_URL%
cd restbed
git checkout %RESTBED_VERSION%
git apply --reject --whitespace=fix %SRC%\restbed\openssl-uwp.patch
git submodule update --init --recursive
git submodule foreach git pull origin master
cd dependency\asio
git apply --reject --whitespace=fix %SRC%\restbed\asio-uwp.patch

cd %SRC%