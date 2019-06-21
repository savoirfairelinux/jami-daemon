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

for /F "tokens=* usebackq" %%F in (`bash -c "pwd | grep /mnt/c/"`) do (
    set NO_AUTO=%%F
)
if "%NO_AUTO%"=="" (
    set ROOTPATH=/c/
) else (
    set ROOTPATH=/mnt/c/
)
set UNIXPATH=%SRC:\=/%
set UNIXPATH=%ROOTPATH%%UNIXPATH:C:/=%

bash -c "%PATCH_CMD% %UNIXPATH%gnutls/gnutls-3.6.7-win32-compat.patch"
bash -c "%PATCH_CMD% %UNIXPATH%gnutls/gnutls-3.6.7-win32-vs-support.patch"
bash -c "%PATCH_CMD% %UNIXPATH%gnutls/read-file-limits.h.patch"

cd %SRC%
