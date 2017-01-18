@echo OFF
SETLOCAL EnableDelayedExpansion

set SRC=%~dp0

set DEPENDENCIES=( ^
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
yaml-cpp, ^
zlib ^
)

for %%I in %DEPENDENCIES% do (
    call %SRC%\%%I\fetch_and_patch.bat
)