set BUILD=%SRC%..\build

set PJPROJECT_VERSION=6b9212dcb4b3f781c1e922ae544b063880bc46ac
set PJPROJECT_URL=https://github.com/pjsip/pjproject/archive/$(PJPROJECT_VERSION).tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\pjproject-%PJPROJECT_VERSION%.tar.bz2 %cd%
) else (
    %WGET_CMD% %PJPROJECT_URL%
)

7z -y x pjproject-%PJPROJECT_VERSION%.tar.bz2 && 7z -y x pjproject-%PJPROJECT_VERSION%.tar -o%BUILD%
del pjproject-%PJPROJECT_VERSION%.tar && del pjproject-%PJPROJECT_VERSION%.tar.bz2
rename %BUILD%\pjproject-%PJPROJECT_VERSION% pjproject

cd %BUILD%\pjproject

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
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/ipv6.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/ice_config.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/multiple_listeners.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/pj_ice_sess.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/fix_turn_fallback.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/fix_ioqueue_ipv6_sendto.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/add_dtls_transport.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/rfc6062.patch"

%APPLY_CMD% %SRC%\pjproject\pj_vs_gnutls.patch
%APPLY_CMD% %SRC%\pjproject\pj_vs_config.patch
%APPLY_CMD% %SRC%\pjproject\pj_vs2017_props.patch

if "%1"=="uwp" (
    %APPLY_CMD% %SRC%\pjproject\pj_uwp.patch
)

cd %SRC%