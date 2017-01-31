set BUILD=%SRC%..\build

set PTHREADS_URL="ftp://sourceware.org/pub/pthreads-win32/pthreads-w32-2-9-1-release.zip"

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\pthreads-w32-2-9-1-release.zip %cd%
) else (
    wget %PTHREADS_URL%
)

7z -y x pthreads-w32-2-9-1-release.zip -o%BUILD%
del pthreads-w32-2-9-1-release.zip
rmdir /Q /S %BUILD%\QueueUserAPCEx && rmdir /Q /S %BUILD%\Pre-built.2
rename %BUILD%\pthreads.2 pthreads

cd %BUILD%\pthreads

git apply --reject --whitespace=fix %SRC%\pthreads\pthreads-uwp.patch

cd %SRC%