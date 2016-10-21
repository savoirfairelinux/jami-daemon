@ECHO OFF
SETLOCAL EnableDelayedExpansion

SET UPSTREAMURL=https://github.com/atraczyk
SET DEPENDENCIES=( ^
opendht, ^
portaudio, ^
libsndfile, ^
libsamplerate, ^
pcre, ^
libupnp, ^
jsoncpp, ^
yaml-cpp, ^
cryptopp, ^
pthreads, ^
boost, ^
pjproject ^
)

SET PASSDEPENDENCIES=%~1

git status >NUL 2>&1
IF %ERRORLEVEL% EQU 128 (
    git init > NUL
) ELSE (
    IF %ERRORLEVEL% EQU 9009 (
        ECHO git not installed.
        EXIT /B 1
    )
)

SET CURRDIR=%~dp1

cd ..\
cd contrib
FOR %%I IN %DEPENDENCIES% DO (
    ECHO !PASSDEPENDENCIES! | FINDSTR /C:"%%I" >NUL 2>&1 || (
        CALL :cloneOrUpdateRepo "%%I" )
)
cd %CURRDIR% >NUL
GOTO exit

:cloneOrUpdateRepo
SET REPONAME=%~1
IF EXIST "%REPONAME%" (
    ECHO %REPONAME%: Existing folder found. Checking for updates...
    cd %REPONAME%
    FOR /f %%J IN ('git rev-parse HEAD') do set CURRHEAD=%%J
    FOR /f %%J IN ('git ls-remote origin HEAD') do set ORIGHEAD=%%J
    IF "!CURRHEAD!"=="!ORIGHEAD!" (
        ECHO %REPONAME%: Repository up to date.
    ) ELSE (
        ECHO %REPONAME%: Updates available. Updating repository...
        git checkout master --quiet
        git stash --quiet
        git pull origin master --quiet -ff
        git stash pop --quiet
    )
    cd ..\
) ELSE (
    ECHO %REPONAME%: Existing folder not found. Cloning repository...
    SET REPOURL=%UPSTREAMURL%/%REPONAME%.git
    git clone !REPOURL! --quiet
    cd %REPONAME%
    git config --local core.autocrlf false
    git rm --cached -r . --quiet
    git reset --hard --quiet
    cd ..\
)

SET PASSDEPENDENCIES=%PASSDEPENDENCIES% %REPONAME%

IF EXIST "%REPONAME%\MSVC\project_get_dependencies.bat" (
    ECHO %REPONAME%: Found additional dependencies...
    ECHO.
    cd %REPONAME%\MSVC
    project_get_dependencies.bat "!PASSDEPENDENCIES!" || GOTO exitOnError
    cd ..\..
)
ECHO.
EXIT /B %ERRORLEVEL%

:exitOnError
cd %CURRDIR%

:exit
(
    ENDLOCAL
    SET PASSDEPENDENCIES=%PASSDEPENDENCIES%
)

ECHO %CMDCMDLINE% | FINDSTR /L %COMSPEC% >NUL 2>&1
IF %ERRORLEVEL% == 0 IF "%~1"=="" PAUSE

EXIT /B 0