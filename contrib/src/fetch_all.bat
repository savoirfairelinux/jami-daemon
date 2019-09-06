@echo off
@setlocal enabledelayedexpansion

echo fetching and patching contrib for %1

if "%USE_CACHE%"=="" (
    set USE_CACHE=0
)

set SRC=%~dp0

set WGET_CMD=wget --no-check-certificate --retry-connrefused --waitretry=1 --read-timeout=20 --timeout=15 --tries=4
set PATCH_CMD=patch -flp1 -i
set APPLY_CMD=git apply --reject --ignore-whitespace --whitespace=fix

if "%1"=="uwp" (
    goto uwpDeps
) else if "%1"=="win32" (
    goto win32Deps
)

:uwpDeps
set DEPENDENCIES=( ^
media-sdk, ^
ffnvcodec, ^
ffmpeg, ^
argon2, ^
zlib ^
fmt, ^
http_parser, ^
restinio, ^
gmp, ^
iconv, ^
jsoncpp, ^
msgpack, ^
nettle, ^
gnutls, ^
opendht, ^
opus, ^
pjproject, ^
portaudio, ^
pthreads, ^
secp256k1, ^
upnp, ^
x264, ^
yaml-cpp, ^
)
goto fetch

:win32Deps
set DEPENDENCIES=( ^
media-sdk, ^
ffnvcodec, ^
ffmpeg, ^
asio, ^
argon2, ^
zlib ^
fmt, ^
http_parser, ^
restinio, ^
gmp, ^
iconv, ^
jsoncpp, ^
msgpack, ^
nettle, ^
gnutls, ^
opendht, ^
openssl, ^
opus, ^
pjproject, ^
portaudio, ^
pthreads, ^
secp256k1, ^
upnp, ^
vpx, ^
x264, ^
yaml-cpp, ^
)

if /I %3 equ "" (
    goto fetch
) else (
    goto fetch_one
)

:fetch
if exist %SRC%\..\build rd /S /Q %SRC%\..\build
for %%I in %DEPENDENCIES% do (
    echo fetching: %%I
    call %SRC%\%%I\fetch_and_patch.bat %1 %2
)
goto cleanup

:fetch_one
set found="N"
for %%I in %DEPENDENCIES% do (
    if /I %3 equ %%I (
        if exist %SRC%\..\build\%%I rd /S /Q %SRC%\..\build\%%I
        echo fetching: %%I
        set found="Y"
        call %SRC%\%%I\fetch_and_patch.bat %1 %2
    )
)
if %found%=="N" (
    echo "%3" not in listed contrib
)

:cleanup
@endlocal