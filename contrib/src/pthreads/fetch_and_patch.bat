set BUILD=%SRC%..\build

set PTHREADS_VERSION="pthreads4w-code-v2.10.0-rc"
set PTHREADS_URL="https://sourceforge.net/projects/pthreads4w/files/%PTHREADS_VERSION%.zip"

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%PTHREADS_VERSION%.zip %cd%
) else (
    wget %PTHREADS_URL%
)

7z -y x %PTHREADS_VERSION%.zip -o%BUILD%
del %PTHREADS_VERSION%.zip
rmdir /Q /S %BUILD%\QueueUserAPCEx && rmdir /Q /S %BUILD%\Pre-built.2
rename %BUILD%\pthreads4w-code-02fecc211d626f28e05ecbb0c10f739bd36d6442 pthreads

cd %BUILD%\pthreads

git apply --reject --whitespace=fix %SRC%\pthreads\pthreads-uwp.patch

cd %SRC%