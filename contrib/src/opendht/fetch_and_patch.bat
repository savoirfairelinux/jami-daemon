set BUILD=%SRC%..\build

set OPENDHT_VERSION=3fb7c2acfe098b18908d1803e5aaa2437d878af4
set OPENDHT_URL=https://github.com/savoirfairelinux/opendht/archive/%OPENDHT_VERSION%.tar.gz

mkdir %BUILD%
wget %OPENDHT_URL%
7z -y x opendht-%OPENDHT_VERSION%.tar.gz && 7z -y x opendht-%OPENDHT_VERSION%.tar -o%BUILD%
del opendht-%OPENDHT_VERSION%.tar && del opendht-%OPENDHT_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\opendht-%OPENDHT_VERSION% opendht

cd %SRC%