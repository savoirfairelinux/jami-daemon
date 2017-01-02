set BUILD=..\..\build
set SRC=..\..\src

mkdir %BUILD%
wget http://www.pjsip.org/release/2.5.5/pjproject-2.5.5.zip
unzip pjproject-2.5.5.zip -d ..\..\build
del pjproject-2.5.5.zip
rename ..\..\build\pjproject-2.5.5 pjproject

cd ..\..\build\pjproject

git apply --reject --whitespace=fix %SRC%\pjproject\intptr_t.patch
git apply --reject --whitespace=fix %SRC%\pjproject\pj_win.patch
git apply --reject --whitespace=fix %SRC%\pjproject\endianness.patch
git apply --reject --whitespace=fix %SRC%\pjproject\gnutls.patch
git apply --reject --whitespace=fix %SRC%\pjproject\notestsapps.patch
git apply --reject --whitespace=fix %SRC%\pjproject\ipv6.patch
git apply --reject --whitespace=fix %SRC%\pjproject\ice_config.patch
git apply --reject --whitespace=fix %SRC%\pjproject\multiple_listeners.patch
git apply --reject --whitespace=fix %SRC%\pjproject\fix_turn_fallback.patch
git apply --reject --whitespace=fix %SRC%\pjproject\fix_ioqueue_ipv6_sendto.patch
git apply --reject --whitespace=fix %SRC%\pjproject\add_dtls_transport.patch
git apply --reject --whitespace=fix %SRC%\pjproject\pj_uwp.patch
git apply --reject --whitespace=fix %SRC%\pjproject\pj_uwp_ice_sess.patch
git apply --reject --whitespace=fix %SRC%\pjproject\pj_uwp_xbox_one.patch

cd %SRC%