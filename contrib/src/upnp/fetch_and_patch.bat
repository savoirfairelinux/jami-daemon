set BUILD=%SRC%..\build

set UPNP_VERSION=1.8.4
set UPNP_URL=https://github.com/mrjimenez/pupnp/archive/release-%UPNP_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\release-%UPNP_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %UPNP_URL%
)

7z -y x release-%UPNP_VERSION%.tar.gz && 7z -y x release-%UPNP_VERSION%.tar -o%BUILD%
del release-%UPNP_VERSION%.tar && del release-%UPNP_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\pupnp-release-%UPNP_VERSION% libupnp

cd %BUILD%\libupnp

cd %SRC%
