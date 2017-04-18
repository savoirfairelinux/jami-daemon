@echo OFF
SETLOCAL EnableDelayedExpansion

set SRC=%~dp0

set DEPENDENCIES=( ^
argon2, ^
boost, ^
cryptopp, ^
ffmpeg, ^
gmp, ^
gnutls, ^
iconv, ^
jsoncpp, ^
msgpack, ^
nettle, ^
opendht, ^
pcre, ^
pjproject, ^
portaudio, ^
pthreads, ^
restbed, ^
samplerate, ^
upnp, ^
x264, ^
yaml-cpp, ^
zlib ^
)

for %%I in %DEPENDENCIES% do (
    call %SRC%\%%I\fetch_and_patch.bat
)