set BUILD=%SRC%..\build

set GNUTLS_VERSION=018a4a655ad784b1ef4f1b311ff031aeed656090
set GNUTLS_URL=https://github.com/ShiftMediaProject/gnutls/archive/%GNUTLS_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%GNUTLS_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %GNUTLS_URL%
)

7z -y x %GNUTLS_VERSION%.tar.gz && 7z -y x %GNUTLS_VERSION%.tar -o%BUILD%
del %GNUTLS_VERSION%.tar && del %GNUTLS_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\gnutls-%GNUTLS_VERSION% gnutls

cd %BUILD%\gnutls

%APPLY_CMD% %SRC%\gnutls\gnutls-no-egd.patch
%APPLY_CMD% %SRC%\gnutls\read-file-limits.h.patch
%APPLY_CMD% %SRC%\gnutls\gnutls-uwp.patch

cd %SRC%
