@echo OFF
SETLOCAL EnableDelayedExpansion

set SRC=%~dp0

set DEPENDENCIES=( ^
gmp, ^
gnutls, ^
iconv, ^
opendht, ^
nettle, ^
pjproject ^
zlib ^
)

for %%I in %DEPENDENCIES% do (
    call %SRC%\%%I\fetch_and_patch.bat
)