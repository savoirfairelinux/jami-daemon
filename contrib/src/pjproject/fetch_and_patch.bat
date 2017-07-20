set BUILD=%SRC%..\build

set PJPROJECT_VERSION=2.5.5
set PJPROJECT_URL=http://www.pjsip.org/release/%PJPROJECT_VERSION%/pjproject-%PJPROJECT_VERSION%.zip

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\pjproject-%PJPROJECT_VERSION%.zip %cd%
) else (
    wget %PJPROJECT_URL%
)

unzip -q pjproject-%PJPROJECT_VERSION%.zip -d %BUILD%
del pjproject-%PJPROJECT_VERSION%.zip
rename %BUILD%\pjproject-%PJPROJECT_VERSION% pjproject

cd %BUILD%\pjproject

git apply --reject --whitespace=fix %SRC%\pjproject\pj_uwp_gnutls.patch
git apply --reject --whitespace=fix %SRC%\pjproject\ipv6.patch
git apply --reject --whitespace=fix %SRC%\pjproject\ice_config.patch
git apply --reject --whitespace=fix %SRC%\pjproject\pj_uwp_multiple_listeners.patch
git apply --reject --whitespace=fix %SRC%\pjproject\fix_ioqueue_ipv6_sendto.patch
git apply --reject --whitespace=fix %SRC%\pjproject\add_dtls_transport.patch
git apply --reject --whitespace=fix %SRC%\pjproject\pj_uwp_ice_sess.patch
git apply --reject --whitespace=fix %SRC%\pjproject\pj_uwp_fix_turn_fallback.patch
git apply --reject --whitespace=fix %SRC%\pjproject\pj_uwp.patch

cd %SRC%