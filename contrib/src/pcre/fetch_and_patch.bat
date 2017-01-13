set BUILD=%SRC%..\build

set PCRE_VERSION=8.40
set PCRE_URL=ftp://ftp.csx.cam.ac.uk/pub/software/programming/pcre
set PCRE_NAME=pcre

mkdir %BUILD%
wget %PCRE_URL%/%PCRE_NAME%-%PCRE_VERSION%.zip
unzip -q %PCRE_NAME%-%PCRE_VERSION%.zip -d %BUILD%
del %PCRE_NAME%-%PCRE_VERSION%.zip
rename %BUILD%\%PCRE_NAME%-%PCRE_VERSION% %PCRE_NAME%

cd %BUILD%\%PCRE_NAME%

git apply --reject --whitespace=fix %SRC%\%PCRE_NAME%\pcre-uwp.patch

cd %SRC%