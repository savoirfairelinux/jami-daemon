set BUILD=%SRC%..\build

set PTHREADS_VERSION="pthreads4w-code-v2.10.0-rc"
set PTHREADS_VERSION2="pthreads4w-code-02fecc211d626f28e05ecbb0c10f739bd36d6442"
set PTHREADS_URL="https://sourceforge.net/projects/pthreads4w/files/%PTHREADS_VERSION%.zip"
set PTHREADS_URL2="https://cfhcable.dl.sourceforge.net/project/pthreads4w/%PTHREADS_VERSION%.zip"

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%PTHREADS_VERSION%.zip %cd%
) else (
    %WGET_CMD% %PTHREADS_URL%
    if %ERRORLEVEL% neq 0 (
        %WGET_CMD% %PTHREADS_URL2%
    )
)

7z -y x %PTHREADS_VERSION%.zip -o%BUILD%
del %PTHREADS_VERSION%.zip
rename %BUILD%\%PTHREADS_VERSION2% pthreads

cd %BUILD%\pthreads

%APPLY_CMD% %SRC%\pthreads\pthreads-windows.patch
%APPLY_CMD% %SRC%\pthreads\pthreads-uwp.patch
%APPLY_CMD% %SRC%\pthreads\pthreads-vs2017.patch

cd %SRC%