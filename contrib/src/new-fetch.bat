@echo OFF
SETLOCAL EnableDelayedExpansion

if "%USE_CACHE%"=="" (
    set USE_CACHE=0
)

set SRC=%~dp0

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

set WGET_CMD=wget --no-check-certificate
set PATCH_CMD=patch -flp1 -i
set APPLY_CMD=git apply --reject --ignore-whitespace --whitespace=fix

for %%I in %DEPENDENCIES% do (
    call %SRC%\%%I\fetch_and_patch.bat
)