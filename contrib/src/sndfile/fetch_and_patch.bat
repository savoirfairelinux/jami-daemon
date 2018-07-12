set BUILD=%SRC%..\build

set SNDFILE_VERSION=1.0.25
set SNDFILE_URL=http://www.mega-nerd.com/libsndfile/files/libsndfile-%SNDFILE_VERSION%.tar.gz

mkdir %BUILD%

if %USE_CACHE%==1 (
    copy %CACHE_DIR%\libsndfile-%SNDFILE_URL%.tar.gz %cd%
) else (
    %WGET_CMD% %SNDFILE_URL%
)

7z -y x libsndfile-%SNDFILE_VERSION%.tar.gz && 7z -y x libsndfile-%SNDFILE_VERSION%.tar -o%BUILD%
del libsndfile-%SNDFILE_VERSION%.tar && del libsndfile-%SNDFILE_VERSION%.tar.gz
rename %BUILD%\libsndfile-%SNDFILE_VERSION% sndfile

cd %BUILD%\sndfile

%APPLY_CMD% %SRC%\sndfile\vs2017.patch

cd %SRC%