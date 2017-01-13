@echo OFF
SETLOCAL EnableDelayedExpansion

set SRC=%~dp0

set DEPENDENCIES=( ^
boost, ^
cryptopp, ^
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
restbed, ^
samplerate, ^
vorbis, ^
yaml-cpp, ^
zlib ^
)

for %%I in %DEPENDENCIES% do (
    call %SRC%\%%I\fetch_and_patch.bat
)