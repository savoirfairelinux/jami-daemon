set BUILD=%SRC%..\build

set MEDIA_SDK_HASH=intel-mediasdk-19.2.0
set MEDIA_SDK_URL=https://github.com/Intel-Media-SDK/MediaSDK/archive/%MEDIA_SDK_HASH%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\%MEDIA_SDK_HASH%.tar.gz %cd%
) else (
    %WGET_CMD% %MEDIA_SDK_URL%
)

7z -y x %MEDIA_SDK_HASH%.tar.gz && 7z -y x %MEDIA_SDK_HASH%.tar -o%BUILD%
del %MEDIA_SDK_HASH%.tar && del %MEDIA_SDK_HASH%.tar.gz && del %BUILD%\pax_global_header
rename %BUILD%\MediaSDK-%MEDIA_SDK_HASH% media-sdk

cd %BUILD%\media-sdk

cd %SRC%