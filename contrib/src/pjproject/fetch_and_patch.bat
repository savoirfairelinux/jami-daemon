set BUILD=%SRC%..\build

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\pjproject-2.5.5.zip %cd%
) else (
    wget http://www.pjsip.org/release/2.5.5/pjproject-2.5.5.zip
)

unzip -q pjproject-2.5.5.zip -d %BUILD%
del pjproject-2.5.5.zip
rename %BUILD%\pjproject-2.5.5 pjproject

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