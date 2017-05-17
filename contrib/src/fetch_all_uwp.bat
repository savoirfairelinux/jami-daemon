@echo OFF
SETLOCAL EnableDelayedExpansion

if "%USE_CACHE%"=="" (
	set USE_CACHE=0
)
 
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
opus, ^
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