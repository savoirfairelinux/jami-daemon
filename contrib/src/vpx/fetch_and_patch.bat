set BUILD=%SRC%..\build

set VPX_VERSION=540af22e9a85798cc6d0b484c85184a5c00270f0
set VPX_URL=https://github.com/ShiftMediaProject/libvpx/archive/%VPX_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%VPX_VERSION%.tar.gz %cd%
) else (
    %WGET_CMD% %VPX_URL%
)

7z -y x %VPX_VERSION%.tar.gz && 7z -y x %VPX_VERSION%.tar -o%BUILD%
del %VPX_VERSION%.tar && del %VPX_VERSION%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\libvpx-%VPX_VERSION% vpx

cd %BUILD%\vpx

%APPLY_CMD% %SRC%\vpx\windows-vpx_config.patch

cd %SRC%