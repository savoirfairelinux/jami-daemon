set BUILD=%SRC%..\build

set OPENDHT_VERSION=20f4355b7345c08ab18aaa9f556b394161a0598d
set OPENDHT_URL=https://github.com/savoirfairelinux/opendht/archive/%OPENDHT_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%OPENDHT_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %OPENDHT_URL%
)

7z -y x %OPENDHT_VERSION%.tar.gz && 7z -y x %OPENDHT_VERSION%.tar -o%BUILD%
del %OPENDHT_VERSION%.tar && del %OPENDHT_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\opendht-%OPENDHT_VERSION% opendht

cd %BUILD%\opendht

%APPLY_CMD% %SRC%\opendht\opendht-vs2017.patch

cd %SRC%
