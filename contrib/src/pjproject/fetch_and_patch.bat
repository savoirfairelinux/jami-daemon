set BUILD=%SRC%..\build

set PJPROJECT_VERSION=5dfa75be7d69047387f9b0436dd9492bbbf03fe4
set PJPROJECT_URL=https://github.com/pjsip/pjproject/archive/%PJPROJECT_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%PJPROJECT_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %PJPROJECT_URL%
)

7z -y x %PJPROJECT_VERSION%.tar.gz && 7z -y x %PJPROJECT_VERSION%.tar -o%BUILD%
del %PJPROJECT_VERSION%.tar && del %PJPROJECT_VERSION%.tar.gz && del %BUILD%\pax_global_header
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
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/fix_turn_alloc_failure.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/ipv6.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/multiple_listeners.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/pj_ice_sess.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/fix_turn_fallback.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/fix_ioqueue_ipv6_sendto.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/add_dtls_transport.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/rfc6544.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/ice_config.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/fix_first_packet_turn_tcp.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/fix_ebusy_turn.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/ignore_ipv6_on_transport_check.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/fix_turn_connection_failure.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/disable_local_resolution.patch"
bash -c "%PATCH_CMD% %UNIXPATH%pjproject/fix_assert_on_connection_attempt.patch"

%APPLY_CMD% %SRC%\pjproject\win32_vs_gnutls.patch
%APPLY_CMD% %SRC%\pjproject\win_config.patch
%APPLY_CMD% %SRC%\pjproject\win_vs2017_props.patch

if "%1"=="uwp" (
    %APPLY_CMD% %SRC%\pjproject\uwp_vs.patch
)

cd %SRC%