git clone --recursive 

set BUILD=%SRC%..\build

set FFMPEG_URL=git://github.com/Microsoft/FFmpegInterop.git

mkdir %BUILD%
cd %BUILD%

git clone --recursive %FFMPEG_URL%

cd %SRC%