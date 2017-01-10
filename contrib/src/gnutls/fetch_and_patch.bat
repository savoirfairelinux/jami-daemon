set BUILD=%SRC%..\build

mkdir %BUILD%
cd %BUILD%
git clone https://github.com/ShiftMediaProject/gnutls.git

cd gnutls

git apply --reject --whitespace=fix %SRC%\gnutls\gnutls-no-egd.patch
git apply --reject --whitespace=fix %SRC%\gnutls\read-file-limits.h.patch
git apply --reject --whitespace=fix %SRC%\gnutls\gnutls-uwp.patch

cd %SRC%