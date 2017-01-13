set BUILD=%SRC%..\build

set YAMLCPP_VERSION=24fa1b33805c9a91df0f32c46c28e314dd7ad96f
set YAMLCPP_URL="https://github.com/jbeder/yaml-cpp.git"

mkdir %BUILD%
cd %BUILD%

git clone %YAMLCPP_URL%
cd yaml-cpp
git checkout %YAMLCPP_VERSION%

git apply --reject --whitespace=fix %SRC%\yaml-cpp\yaml-cpp-uwp.patch

cd %SRC%