set BUILD=%SRC%..\build

set GNUTLS_VERSION=f2d0ade53ff644da55244aed79d05eca78d11a2f
set GNUTLS_URL=https://github.com/ShiftMediaProject/gnutls/%GNUTLS_URL%.tar.gz

mkdir %BUILD%
wget %GNUTLS_URL%
7z -y x gnutls-%GNUTLS_VERSION%.tar.gz && 7z -y x gnutls-%GNUTLS_VERSION%.tar -o%BUILD%
del gnutls-%GNUTLS_VERSION%.tar && del gnutls-%GNUTLS_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\gnutls-%GNUTLS_VERSION% gnutls

cd %BUILD%\gnutls

git apply --reject --whitespace=fix %SRC%\gnutls\gnutls-no-egd.patch
git apply --reject --whitespace=fix %SRC%\gnutls\read-file-limits.h.patch
git apply --reject --whitespace=fix %SRC%\gnutls\gnutls-uwp.patch

cd %SRC%