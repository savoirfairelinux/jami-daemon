set BUILD=%SRC%..\build

set PA_VERSION=v190600_20161030
set PA_URL="http://www.portaudio.com/archives/pa_stable_%PA_VERSION%.tgz"

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\pa_stable_%PA_VERSION%.tgz %cd%
) else (
    wget %PA_URL%
)

7z -y e pa_stable_%PA_VERSION%.tgz && 7z -y x pa_stable_%PA_VERSION%.tar -o%BUILD%
del pa_stable_%PA_VERSION%.tgz && del pa_stable_%PA_VERSION%.tar

cd %BUILD%\portaudio

git apply --reject --whitespace=fix %SRC%\portaudio\pa-uwp.patch

cd %SRC%