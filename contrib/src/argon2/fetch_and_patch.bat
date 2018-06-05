set BUILD=%SRC%..\build

set ARGON2_VERSION=670229c849b9fe882583688b74eb7dfdc846f9f6
set ARGON2_URL=https://github.com/P-H-C/phc-winner-argon2/archive/%ARGON2_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%ARGON2_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %ARGON2_URL%
)

7z -y x %ARGON2_VERSION%.tar.gz && 7z -y x %ARGON2_VERSION%.tar -o%BUILD%
del %ARGON2_VERSION%.tar && del %ARGON2_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\phc-winner-argon2-%ARGON2_VERSION% argon2

cd %BUILD%\argon2

git apply --reject --whitespace=fix %SRC%\argon2\argon2-uwp.patch

cd %SRC%