@echo OFF
SETLOCAL EnableDelayedExpansion

set SRC=%~dp0

set DEPENDENCIES=( ^
boost, ^
cryptopp, ^
ffmpeg, ^
flac, ^
gmp, ^
gnutls, ^
iconv, ^
jsoncpp, ^
msgpack, ^
nettle, ^
opendht, ^
ogg, ^
pcre, ^
pjproject, ^
portaudio, ^
pthreads, ^
restbed, ^
samplerate, ^
upnp, ^
vorbis, ^
yaml-cpp, ^
zlib ^
)

for %%I in %DEPENDENCIES% do (
    call %SRC%\%%I\fetch_and_patch.bat
)