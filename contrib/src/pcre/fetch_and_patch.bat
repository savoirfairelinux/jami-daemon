set BUILD=%SRC%..\build

set PCRE_VERSION=8.42
set PCRE_URL=ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre
set PCRE_NAME=pcre

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%PCRE_NAME%-%PCRE_VERSION%.tar.bz2 %cd%
) else (
    %WGET_CMD% %PCRE_URL%/%PCRE_NAME%-%PCRE_VERSION%.tar.bz2
)

7z -y x %PCRE_NAME%-%PCRE_VERSION%.tar.bz2 && 7z -y x %PCRE_NAME%-%PCRE_VERSION%.tar -o%BUILD%
del %PCRE_NAME%-%PCRE_VERSION%.tar && del %PCRE_NAME%-%PCRE_VERSION%.tar.bz2
rename %BUILD%\%PCRE_NAME%-%PCRE_VERSION% %PCRE_NAME%

cd %BUILD%\%PCRE_NAME%

mkdir msvc && cd msvc
setlocal
set PATH=C:\\Program Files\\CMake\\bin\\;%PATH%
cmake .. -G "Visual Studio 15 2017 Win64"
endlocal

cd %SRC%