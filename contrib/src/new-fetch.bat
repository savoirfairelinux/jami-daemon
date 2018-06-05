@echo OFF
SETLOCAL EnableDelayedExpansion

if "%USE_CACHE%"=="" (
	set USE_CACHE=0
)
 
set SRC=%~dp0

set DEPENDENCIES=( ^
restbed, ^
)

set WGET_CMD=wget --no-check-certificate

for %%I in %DEPENDENCIES% do (
    call %SRC%\%%I\fetch_and_patch.bat
)