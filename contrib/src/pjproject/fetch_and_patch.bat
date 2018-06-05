set BUILD=%SRC%..\build

set PJPROJECT_VERSION=2.7.2
set PJPROJECT_URL=http://www.pjsip.org/release/%PJPROJECT_VERSION%/pjproject-%PJPROJECT_VERSION%.tar.bz2

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

set UNIXPATH=%SRC:\=/%
set UNIXPATH=%UNIXPATH:C:=/mnt/c%
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/gnutls.patch"
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

%APPLY_CMD% %SRC%\pjproject\pj_uwp.patch

cd %SRC%