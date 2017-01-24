set BUILD=%SRC%..\build

set GMP_VERSION=3c8f5a0ae0c2ac9ff0ea31b27f71b152979b556d
set GMP_URL="https://github.com/ShiftMediaProject/gmp.git"

mkdir %BUILD%
cd %BUILD%

git clone %GMP_URL%
cd %GMP_URL%
git checkout %GMP_VERSION%

cd gmp
git apply --reject --whitespace=fix %SRC%\gmp\gmp-uwp.patch

cd %SRC%