:: Ring - native Windows fetch and build

@echo off
@setlocal

if "%1" == "/?" goto Usage
if "%~1" == "" goto Usage

set doFetch=N
set doBuildContrib=N
set doBuildDaemon=N
set targetContrib=""

set SCRIPTNAME=%~nx0

if "%1"=="fetch" (
    set doFetch=Y
) else if "%1"=="contrib" (
    set doBuildContrib=Y
) else if "%1"=="daemon" (
    set doBuildDaemon=Y
) else (
    goto Usage
)

set BUILD.x86=N
set BUILD.x64=Y
set BUILD.uwp=N
set BUILD.win32=Y

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
) else if /I "%1" neq "" (
    if "%doBuildContrib%" neq "N" (
        set targetContrib=%1
    ) else if "%doFetch%" neq "N" (
        set targetContrib=%1
    )
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
    if "%arch%"=="x86" (
        set DAEMON_MSBUILD_ARGS=/nologo /p:useenv=true /p:Configuration=ReleaseLib /p:Platform=Win32 /verbosity:normal /maxcpucount:%NUMBER_OF_PROCESSORS%
    ) else if "%arch%"=="x64" (
        set DAEMON_MSBUILD_ARGS=/nologo /p:useenv=true /p:Configuration=ReleaseLib /p:Platform=x64 /verbosity:normal /maxcpucount:%NUMBER_OF_PROCESSORS%
    )
) else if "%BUILD.win32%"=="Y" (
    set platform=win32
    if "%arch%"=="x86" (
        set DAEMON_MSBUILD_ARGS=/nologo /p:useenv=true /p:Configuration=ReleaseLib_win32 /p:Platform=Win32 /verbosity:normal /maxcpucount:%NUMBER_OF_PROCESSORS%
    ) else if "%arch%"=="x64" (
        set DAEMON_MSBUILD_ARGS=/nologo /p:useenv=true /p:Configuration=ReleaseLib_win32 /p:Platform=x64 /verbosity:normal /maxcpucount:%NUMBER_OF_PROCESSORS%
    )
)
if "%arch%" neq "N" (
    if "%platform%" neq "N" (
        if "%doFetch%" neq "N" (
            call %CONTRIB_DIR%\src\fetch_all.bat %platform% %arch% %targetContrib%
        ) else if "%doBuildContrib%" neq "N" (
			call %CONTRIB_DIR%\build_all.bat %platform% %arch% %targetContrib%
        ) else if "%doBuildDaemon%" neq "N" (
            goto buildDaemon
        )
        goto :eof
    )
)
goto Usage

:buildDaemon
@setlocal

set VSInstallerFolder="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
if %PROCESSOR_ARCHITECTURE%==x86 set VSInstallerFolder="%ProgramFiles%\Microsoft Visual Studio\Installer"

pushd %VSInstallerFolder%
for /f "usebackq tokens=*" %%i in (`vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
  set VSLATESTDIR=%%i
)
popd

echo VS Installation folder: %VSLATESTDIR%

if not exist "%VSLATESTDIR%\VC\Auxiliary\Build\vcvarsall.bat" (
    echo:
    echo VSInstallDir not found or not installed correctly.
    goto cleanup
)

if %PROCESSOR_ARCHITECTURE%==x86 (
    if "%platform%"=="uwp" (
        set Comp_x86=x86 uwp 10.0.15063.0
        set Comp_x64=x86_amd64 uwp 10.0.15063.0
    ) else (
        set Comp_x86=x86 10.0.15063.0
        set Comp_x64=x86_amd64 10.0.15063.0
    )
) else (
    if "%platform%"=="uwp" (
        set Comp_x86=amd64_x86 uwp 10.0.15063.0
        set Comp_x64=amd64 uwp 10.0.15063.0
    ) else (
        set Comp_x86=amd64_x86 10.0.15063.0
        set Comp_x64=amd64 10.0.15063.0
    )
)

set path=%path:"=%
if "%arch%"=="x86" (
    call "%VSLATESTDIR%"\\VC\\Auxiliary\\Build\\vcvarsall.bat %Comp_x86%
) else if "%arch%"=="x64" (
    call "%VSLATESTDIR%"\\VC\\Auxiliary\\Build\\vcvarsall.bat %Comp_x64%
)

::build the daemon
echo "building daemon..."
msbuild ring-daemon.vcxproj %DAEMON_MSBUILD_ARGS%
goto :eof

@endlocal

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
echo     %SCRIPTNAME% fetch win32 x86   - fetch contrib for a win32/x86 build
echo     %SCRIPTNAME% contrib uwp x64   - build uwp(win10)/x64 contrib
echo     %SCRIPTNAME% daemon uwp x64    - build uwp(win10)/x64 daemon
echo:
goto :eof

@endlocal