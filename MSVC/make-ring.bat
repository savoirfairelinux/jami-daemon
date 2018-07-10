:: Ring - native Windows fetch and build

@echo off
@setlocal

if "%1" == "/?" goto Usage
if "%~1" == "" goto Usage

set doFetch=N
set doBuild=N

set SCRIPTNAME=%~nx0

if "%1"=="fetch" (
    set doFetch=Y
) else if "%1"=="build" (
    set doBuild=Y
) else (
    goto Usage
)

set BUILD.x86=N
set BUILD.x64=N
set BUILD.uwp=N
set BUILD.win32=N

shift
:ParseArgs
if "%1" == "" goto FinishedArgs
if /I "%1"=="x86" (
    set BUILD.x86=Y
) else if /I "%1"=="x64" (
    set BUILD.x64=Y
) else if /I "%1"=="uwp" (
    set BUILD.uwp=Y
) else if /I "%1"=="win32" (
    set BUILD.win32=Y
) else (
        goto Usage
)
shift
goto ParseArgs

:FinishedArgs
set CONTRIB_DIR=%~dp0../contrib
set platform=N
set arch=N
if "%BUILD.x86%"=="Y" (
    set arch=x86
) else if "%BUILD.x64%"=="Y" (
    set arch=x64
)
if "%BUILD.uwp%"=="Y" (
    set platform=uwp
) else if "%BUILD.win32%"=="Y" (
    set platform=win32
)
if "%arch%" neq "N" (
    if "%platform%" neq "N" (
        if "%doFetch%" neq "N" (
            call %CONTRIB_DIR%\src\fetch_all.bat %platform% %arch%
        ) else if "%doBuild%" neq "N" (
            call %CONTRIB_DIR%\build_all.bat %platform% %arch%
        )
        goto :eof
    )
)
goto Usage

:Usage
echo:
echo The correct usage is:
echo:
echo     %0 [action] [target platform] [architecture]
echo:
echo where
echo:
echo [action]           is: fetch ^| build
echo [target platform]  is: uwp   ^| win32
echo [architecture]     is: x86   ^| x64
echo:
echo For example:
echo     %SCRIPTNAME% fetch win32 x86   - fetch for a win32/x86 build
echo     %SCRIPTNAME% build uwp x64     - build uwp(win10)/x64 contrib and daemon
echo:
goto :eof

@endlocal