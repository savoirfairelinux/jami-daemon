set BUILD=%SRC%..\build

set GNUTLS_VERSION=3.6.7
set GNUTLS_URL=https://www.gnupg.org/ftp/gcrypt/gnutls/v3.6/gnutls-%GNUTLS_VERSION%.tar.xz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\gnutls-%GNUTLS_VERSION%.tar.xz %cd%
) else (
    %WGET_CMD% %GNUTLS_URL%
)

7z -y x gnutls-%GNUTLS_VERSION%.tar.xz && 7z -y x gnutls-%GNUTLS_VERSION%.tar -o%BUILD%
del gnutls-%GNUTLS_VERSION%.tar && del gnutls-%GNUTLS_VERSION%.tar.xz
rename %BUILD%\gnutls-%GNUTLS_VERSION% gnutls

cd %BUILD%\gnutls

%APPLY_CMD% %SRC%\gnutls\read-file-limits.h.patch

if "%1"=="uwp" (
    %APPLY_CMD% %SRC%\gnutls\gnutls-uwp.patch
)

rem %APPLY_CMD% %SRC%\gnutls\gnutls-mscver.patch

cd %SRC%
