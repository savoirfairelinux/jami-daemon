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
ffmpeg, ^
argon2, ^
zlib ^
gmp, ^
iconv, ^
jsoncpp, ^
msgpack, ^
nettle, ^
gnutls, ^
opendht, ^
opus, ^
pcre, ^
pjproject, ^
portaudio, ^
pthreads, ^
restbed, ^
samplerate, ^
secp256k1, ^
upnp, ^
x264, ^
yaml-cpp, ^
)
goto fetch

:win32Deps
set DEPENDENCIES=( ^
ffmpeg, ^
argon2, ^
zlib ^
gmp, ^
iconv, ^
jsoncpp, ^
msgpack, ^
nettle, ^
gnutls, ^
opendht, ^
opus, ^
pcre, ^
pjproject, ^
portaudio, ^
pthreads, ^
restbed, ^
samplerate, ^
secp256k1, ^
upnp, ^
vpx, ^
x264, ^
yaml-cpp, ^
)
goto fetch

:fetch
for %%I in %DEPENDENCIES% do (
    call %SRC%\%%I\fetch_and_patch.bat %1 %2
)

@endlocal