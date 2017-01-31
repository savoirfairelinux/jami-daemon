set BUILD=%SRC%..\build

set PCRE_VERSION=8.40
set PCRE_URL=ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre
set PCRE_NAME=pcre

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%PCRE_NAME%-%PCRE_VERSION%.zip %cd%
) else (
    wget %PCRE_URL%/%PCRE_NAME%-%PCRE_VERSION%.zip
)

unzip -q %PCRE_NAME%-%PCRE_VERSION%.zip -d %BUILD%
del %PCRE_NAME%-%PCRE_VERSION%.zip
rename %BUILD%\%PCRE_NAME%-%PCRE_VERSION% %PCRE_NAME%

cd %BUILD%\%PCRE_NAME%

git apply --reject --whitespace=fix %SRC%\%PCRE_NAME%\pcre-uwp.patch

rename config.h.generic config.h
rename pcre.h.in pcre.h
rename pcre_chartables.c.dist pcre_chartables.c

cd %SRC%